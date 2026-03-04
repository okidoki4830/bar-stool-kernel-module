#include "groups.h"
#include "metrics.h"
#include "bar_mutex.h"
#include "tables.h"
#include "servers.h"

extern struct metrics bar_metrics;

// Initialize the waiting list
void init_waiting_list(struct waiting_list *wl) {
    INIT_LIST_HEAD(&wl->head);      // Leverage INIT_LIST_HEAD to init linked list head
    wl->total_serviced = 0;
    wl->total_left = 0;
}

struct group *create_group(int id, int num_customers, int stay_duration, int spending, int waiting_time) {
    struct group *g = kmalloc(sizeof(struct group), GFP_KERNEL);
    if (!g) return NULL;

    g->id = id;
    g->customers = num_customers;
    g->stay_duration = stay_duration;
    g->spending = spending;
    g->waiting_time = waiting_time;
    g->arrival_time = jiffies;
    g->is_processed = false;
    INIT_LIST_HEAD(&g->list);

    return g;
}

// Add a group to the waiting list
void add_group(struct waiting_list *wl, struct group *g) {

    list_add_tail(&g->list, &wl->head); // Add group to end of list as they come in the bar
}

// Remove a group from the waiting list and add that group to metrics
void remove_group(struct waiting_list *wl, struct group *g, int serviced) {


  // lockdep_assert_held(&waiting_list_mutex);


   if (!g) {
        printk(KERN_ERR "remove_group: Attempted to remove a NULL group!\n");
        return;
    }

   printk(KERN_INFO "Attempting to remove group ID %d\n", g->id);

  // Remove the groups internal list
    list_del(&g->list);

    printk(KERN_INFO "Group ID %d removed from list\n", g->id);

    mutex_lock(&metrics_mutex);
    printk(KERN_INFO "metrics_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    // Check if they were serviced and increment accordingly
    if (serviced)
    {
        wl->total_serviced++;
        update_metrics_served(&bar_metrics, g);
    }
    else
    {
        wl->total_left++;
        update_metrics_left(&bar_metrics);
    }
    mutex_unlock(&metrics_mutex);
    printk(KERN_INFO "metrics_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

    // Free allocated memory
    kfree(g);

    printk(KERN_INFO "Group ID %d freed\n", g->id);
}

// Check for expired groups and remove them from the waiting list
void check_expired_groups(struct waiting_list *wl) {
    struct group *g;
    unsigned long current_time = jiffies;


    printk(KERN_INFO "Checking expired groups...\n");
     // Iterate through the list as long as it has elements
    while(!list_empty(&wl->head)) {
        g = list_first_entry(&wl->head, struct group, list);

        // time_after() checks if current time is > than arrival + waiting time (the total time of the group)
        if (time_after(current_time, g->arrival_time + (g->waiting_time * HZ))) {
            printk(KERN_INFO "Group %d expired (waited %lu jiffies)\n",
                   g->id, current_time - g->arrival_time);
            remove_group(wl, g, 0);     // Remove the group from the waiting list
        } else {
            printk(KERN_INFO "No expired groups found\n");
            break;
        }
    }
   // mutex_unlock(&waiting_list_mutex);
  //  printk(KERN_INFO "waiting_list_mutex unlocked in checked expired groups function\n");
}

void check_expired_seated_groups(struct table_manager *tm, struct list_head *seated_groups)
{
    if (list_empty(seated_groups)) {
        return;  // No seated groups to process
    }
    struct seated_group *sg, *temp;
    unsigned long current_time = jiffies;

    mutex_lock(&seated_groups_mutex);
    printk(KERN_INFO "seated_groups_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    list_for_each_entry_safe(sg, temp, seated_groups, list) {
        if (sg->expiration <= current_time) {  // Group has expired
            printk(KERN_INFO "Group %d has expired and is leaving\n", sg->grp->id);

            mutex_lock(&table_manager_mutex);
             printk(KERN_INFO "table_manager_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
            // Set stools to dirty for the group
            for (int i = 0; i < NUM_TABLES; i++) {
                for (int j = 0; j < STOOLS_PER_TABLE; j++) {
                    if (tm->tables[i].stools[j] == sg->grp->id) {
                        tm->tables[i].stools[j] = STOOL_DIRTY;  // Mark stool as dirty
                    }
                }
            }
             mutex_unlock(&table_manager_mutex);
                printk(KERN_INFO "table_manager_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
            // Remove the group from the seated groups list
            list_del(&sg->list);

            kfree(sg);  // Free memory for the seated group
        }
    }
    mutex_unlock(&seated_groups_mutex);
    printk(KERN_INFO "seated_groups_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
}


void cleanup_groups(struct waiting_list *wl) {
    printk(KERN_INFO "Cleaning up waiting list groups...\n");

    if (mutex_trylock(&waiting_list_mutex)) {
        printk(KERN_INFO "waiting_list_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
       // printk(KERN_INFO "Mutex locked in cleanup_groups");

        struct group *g, *tmp;
        list_for_each_entry_safe(g, tmp, &wl->head, list) {
            printk(KERN_INFO "Removing group ID: %d", g->id);
            list_del(&g->list);
            update_metrics_left(&bar_metrics);
            kfree(g);
        }

        printk(KERN_INFO "All groups freed, unlocking mutex");
        mutex_unlock(&waiting_list_mutex);
        printk(KERN_INFO "waiting_list_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

    } else {
        printk(KERN_ERR "Failed to acquire waiting_list_mutex after retry\n");
    }
}
/**
 * select_group_from_waiting_list - Selects a group from the waiting list based on priority
 * @wl: Pointer to the waiting list
 * @available_stools: Number of available stools that can be allocated
 *
 * This function implements the group selection algorithm from the waiting list.
 * The current algorithm prioritizes groups with higher spending amounts, but
 * only selects groups whose customer count can be accommodated by available stools.
 *
 * Return: Pointer to the selected group, or NULL if no suitable group is found
 */


struct group *select_group_from_waiting_list(struct waiting_list *wl, struct table_manager *tm) {
    struct group *g, *selected = NULL;

    if (list_empty(&wl->head)) {
        return NULL;
    }

    // Prioritize high-spending groups that also have seating available
    list_for_each_entry(g, &wl->head, list) {
        if (!selected || g->spending > selected->spending) {
            if (find_and_reserve_stools(tm, g->customers, g->id)) {
                selected = g;
            }
        }
    }

    if (selected) {
        list_del(&selected->list);
        printk(KERN_INFO "Selected group %d with spending %d from waiting list\n", selected->id, selected->spending);
    }

    return selected;
}

void seat_group(struct table_manager *tm, int group_id) {
    int i, j;

    mutex_lock(&table_manager_mutex);
    printk(KERN_INFO "table_manager_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    printk(KERN_INFO "Seating group %d\n", group_id);

    for (i = 0; i < NUM_TABLES; i++) {
        for (j = 0; j < STOOLS_PER_TABLE; j++) {
            if (tm->tables[i].reserved_group_id[j] == group_id) {
                tm->tables[i].stools[j] = group_id;  // Assign group ID to reserved stool
                tm->tables[i].reserved_group_id[j] = 0; // Clear reservation
                tm->tables[i].occupied_count++;       // Increment occupied count
            }
        }
    }

    mutex_unlock(&table_manager_mutex);
    printk(KERN_INFO "table_manager_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
}

/**
 * is_group_expired - Checks if a group has exceeded its maximum waiting time
 * @g: Pointer to the group to check
 *
* This function determines if a group has been waiting longer than their 
 * specified maximum waiting time and should leave the bar.
 *
 * Return: 1 if the group has expired (should leave), 0 otherwise
 */
 int is_group_expired(struct group *g) {
    unsigned long current_time = jiffies;
    // time_after() checks if current time is greater than arrival + waiting time
    return time_after(current_time, g->arrival_time + (g->waiting_time * HZ));
}

/**
 * get_waiting_list_status - Generates a status report of the waiting list
 * @wl: Pointer to the waiting list
 * @buffer: Buffer to store the status report
 * @len: Pointer to the current length of the buffer
 *
 * This function collects information about the waiting list including
 * serviced groups, groups that left, and currently waiting groups.
 * The information is formatted for display in the proc file system.
 */
void get_waiting_list_status(struct waiting_list *wl, char *buffer, int *len) {
    struct group *g;
    int first = 1;

    // Lock the waiting list before accessing
    mutex_lock(&waiting_list_mutex);

   // printk(KERN_INFO "waiting_list_mutex locked in get_waiting_list function\n");

    *len += sprintf(buffer + *len, "Waiting list: ");

    // Iterate through waiting groups safely
    list_for_each_entry(g, &wl->head, list) {
        if (!first) {
            *len += sprintf(buffer + *len, ", ");
        } else {
            first = 0;
        }
        *len += sprintf(buffer + *len, "%d{%d}", g->id, g->customers);
    }

    if (first) {
        *len += sprintf(buffer + *len, "[Empty]");
    }
    *len += sprintf(buffer + *len, "\n");

    // Unlock after all list operations
    mutex_unlock(&waiting_list_mutex);
   // printk(KERN_INFO "waiting_list_mutex unlocked in get_waiting_list function\n");
}

