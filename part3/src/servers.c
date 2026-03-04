#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "servers.h"
#include "groups.h"
#include "tables.h"
#include "metrics.h"
#include "bar_mutex.h"

#define NUM_SERVERS 2
#define GUIDING_TIME_MS 2000
#define CLEANING_TIME_MS 2000

//struct server *s = (struct server *)data;
static struct server servers[NUM_SERVERS];
//static struct table_manager *tm = s->tm;
static LIST_HEAD(seated_groups);
static DEFINE_MUTEX(server_state_mutex);
//static DEFINE_MUTEX(seated_groups_mutex);


int server_loop(void *data)
{
    struct server *s = (struct server *)data;
   // static struct table_manager *tm = s->tm;

    while (!kthread_should_stop()) {
        // bar state check
        if (msleep_interruptible(100))
            break;

        mutex_lock(&bar_state_mutex);
        printk(KERN_INFO "bar_state_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
        if (!bar_is_open) {
            mutex_unlock(&bar_state_mutex);
            printk(KERN_INFO "bar_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
            break;
        }
        mutex_unlock(&bar_state_mutex);
        printk(KERN_INFO "bar_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

        // group processing
        struct group *group = NULL;
        mutex_lock(&server_state_mutex);
        printk(KERN_INFO "server_state_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
        s->state = SERVER_IDLE;
        s->current_group_id = -1;
        mutex_unlock(&server_state_mutex);
        printk(KERN_INFO "server_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

        // loch waiting list mutex before using
        if (mutex_lock_interruptible(&waiting_list_mutex)) {
            printk(KERN_INFO "waiting_list_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
            if (kthread_should_stop()) break;
            continue;
        }
        check_expired_seated_groups(&bar_tables, &seated_groups);

        // Check for expired groups before selecting a new group
        check_expired_groups(&waiting_groups);

        group = select_group_from_waiting_list(&waiting_groups, &bar_tables);


        mutex_unlock(&waiting_list_mutex);
        printk(KERN_INFO "waiting_list_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);


        if (group) {
           // printk(KERN_INFO "Server: Selected group %d for guiding, stay_duration=%d\n", group->id, group->stay_duration);
            mutex_lock(&server_state_mutex);
            printk(KERN_INFO "server_state_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
           // printk(KERN_INFO "Server %d: Acquired server_state_mutex, updating state to SERVER_GUIDING\n", s->id);
            s->state = SERVER_GUIDING;
            s->current_group_id = group->id;
            mutex_unlock(&server_state_mutex);
            printk(KERN_INFO "server_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

            unsigned long guide_timeout = msecs_to_jiffies(GUIDING_TIME_MS);

            printk(KERN_INFO "Server %d: Starting guide timeout, guide_timeout=%lu jiffies\n", s->id, guide_timeout);
            while (guide_timeout && !kthread_should_stop()) {
                guide_timeout = schedule_timeout_interruptible(guide_timeout);
            }
            printk(KERN_INFO "Server %d: Guide timeout finished, kthread_should_stop=%d\n", s->id, kthread_should_stop());

            if (!kthread_should_stop()) {
                // seat the group    
                seat_group(&bar_tables, group->id); // display
                struct seated_group *sg = kmalloc(sizeof(*sg), GFP_KERNEL);
                if (sg) {
                    //printk(KERN_INFO "Server %d: Allocated seated_group for group %d\n", s->id, group->id);
                    INIT_LIST_HEAD(&sg->list);
                    sg->grp = group;
                    sg->expiration = jiffies + group->stay_duration * HZ;
                    group->is_processed = true;

                  mutex_lock(&seated_groups_mutex);
                    printk(KERN_INFO "seated_groups_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
                    list_add_tail(&sg->list, &seated_groups);
                    mutex_unlock(&seated_groups_mutex);
                    printk(KERN_INFO "seated_groups_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

                    mutex_lock(&metrics_mutex);
                    printk(KERN_INFO "metrics_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
                    update_metrics_served(&bar_metrics, group);
                    mutex_unlock(&metrics_mutex);
                    printk(KERN_INFO "metrics_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
                }
            }
            kfree(group);
        }
        check_expired_seated_groups(&bar_tables, &seated_groups);

        //  Table cleaning
        for (int i = 0; i < NUM_TABLES; i++) {
            if (kthread_should_stop()) break;

            if (mutex_lock_interruptible(&table_mutex[i])) {
                printk(KERN_INFO "table_mutex %d locked by: %p, PID: %d, Name: %s\n", i, current, current->pid, current->comm);
                if (kthread_should_stop()) break;
                continue;
            }

            if (is_table_empty(&bar_tables, i) && is_table_dirty(&bar_tables, i)) {
                printk(KERN_INFO "Server %d: found table to clean\n", s->id);
                mutex_lock(&server_state_mutex);
                printk(KERN_INFO "server_state_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
                s->state = SERVER_CLEANING;
                s->current_table_id = i;

               mutex_unlock(&server_state_mutex);
                printk(KERN_INFO "server_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

                // Make cleaning sleep interruptible
                unsigned long clean_timeout = msecs_to_jiffies(CLEANING_TIME_MS);
                while (clean_timeout && !kthread_should_stop()) {
                    clean_timeout = schedule_timeout_interruptible(clean_timeout);
                }

                if (!kthread_should_stop()) {
                    clean_table(&bar_tables, i);
                }
            }
            mutex_unlock(&table_mutex[i]);
            printk(KERN_INFO "table_mutex %d unlocked by: %p, PID: %d, Name: %s\n", i, current, current->pid, current->comm);
        }

    }

    // Cleanup
    printk(KERN_INFO "Server %d: Stopping\n", s->id);
    mutex_lock(&server_state_mutex);
    printk(KERN_INFO "server_state_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    s->state = SERVER_IDLE;
    s->current_group_id = -1;
    s->current_table_id = -1;
    mutex_unlock(&server_state_mutex);
    printk(KERN_INFO "server_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    return 0;
}

void get_server_statuses(char *buffer, int *len)
{
    mutex_lock(&server_state_mutex);
    for (int i = 0; i < NUM_SERVERS; i++) {
        const char *status;
        switch (servers[i].state) {
            case SERVER_IDLE:
                status = "idle";
                break;
                case SERVER_GUIDING:
                    *len += sprintf(buffer + *len, "Server %d: guiding Group %d\n",
                                  servers[i].id + 1, servers[i].current_group_id);
                    continue;
                case SERVER_CLEANING:
                    *len += sprintf(buffer + *len, "Server %d: cleaning Table %d\n",
                                  servers[i].id + 1, servers[i].current_table_id);
                   continue;
                default:
                    status = "unknown";
            }
            *len += sprintf(buffer + *len, "Server %d: %s\n", servers[i].id + 1, status);
        }
        mutex_unlock(&server_state_mutex);
    }

    int init_servers(void)
    {
        int i;
    //    init_tables(&tm);


        for (i = 0; i < NUM_SERVERS; i++) {
            servers[i].id = i;
            servers[i].current_group_id = -1;
            servers[i].current_table_id = -1;
            servers[i].tm = &bar_tables; // Set the table manager reference
            servers[i].thread = kthread_run(server_loop, &servers[i], "bar_server_%d", i);
            if (IS_ERR(servers[i].thread)) {
                printk(KERN_ERR "Failed to start server %d\n", i);
                return PTR_ERR(servers[i].thread);
            }
        }
        return 0;
    }

void cleanup_servers(void)
{
    int i;
    printk(KERN_INFO "Cleaning up servers...\n");

    for (i = 0; i < NUM_SERVERS; i++) {
        if (servers[i].thread && !IS_ERR(servers[i].thread)) {
            printk(KERN_INFO "Stopping server %d\n", i);
            kthread_stop(servers[i].thread);
            servers[i].thread = NULL;
            printk(KERN_INFO "Server %d stopped\n", i);
        }
    }

    mutex_lock(&seated_groups_mutex);
    printk(KERN_INFO "seated_groups_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    struct seated_group *curr, *temp;
    list_for_each_entry_safe(curr, temp, &seated_groups, list) {
        list_del(&curr->list);
        printk(KERN_INFO "Freeing seated group %d\n", curr->grp->id);
        kfree(curr->grp);
      kfree(curr);
    }
    mutex_unlock(&seated_groups_mutex);
    printk(KERN_INFO "seated_groups_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    printk(KERN_INFO "Servers cleaned up successfully\n");
}
