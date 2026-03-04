#include "bar_module.h"
#include "metrics.h"
#include "tables.h"
#include "bar_mutex.h"
#include "servers.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/errno.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A Thomas, C Prado, J Kim");
MODULE_DESCRIPTION("Bar Stool Management Module");


extern long (*STUB_open_bar)(void);
extern long (*STUB_bar_group_arrive)(int, int, int, int, int);
extern long (*STUB_close_bar)(void);

struct metrics bar_metrics;
static struct proc_dir_entry *bar_proc_entry;
struct table_manager bar_tables;
struct waiting_list waiting_groups;
//static struct server s;


static char proc_buffer[2048];
int bar_is_open = 0;
static int bar_proc_show(struct seq_file *m, void *v)
{

    int len = 0;

    // Bar state
    mutex_lock(&bar_state_mutex);
   // printk(KERN_INFO "bar_state_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    seq_printf(m, "Bar: %s\n", bar_is_open ? "open" : "closed");
    //seq_printf(m, "DEBUG: bar_is_open value: %d\n", bar_is_open); // test
    mutex_unlock(&bar_state_mutex);
   // printk(KERN_INFO "bar_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

    // Waiting list
    get_waiting_list_status(&waiting_groups, proc_buffer, &len);
    seq_printf(m, "%s", proc_buffer);

    // Tables
    len = 0;

    get_table_status(&bar_tables, proc_buffer, &len);
    seq_printf(m, "%s", proc_buffer);


    // Server statuses
    len = 0;

    get_server_statuses(proc_buffer, &len);
    seq_printf(m, "%s", proc_buffer);


    // Metrics
    mutex_lock(&metrics_mutex);
   // printk(KERN_INFO "metrics_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    seq_printf(m, "Total number of groups served: %d\n", bar_metrics.served_groups);
    seq_printf(m, "Total number of customers served: %d\n", bar_metrics.served_customers);
    seq_printf(m, "Total number of groups left: %d\n", bar_metrics.left_groups);
    seq_printf(m, "Profit: %lu\n", bar_metrics.total_profit);

    // Review rating
    int total_groups = bar_metrics.served_groups + bar_metrics.left_groups;
    if (total_groups == 0) {
        seq_printf(m, "Rating: 0.000\n");
    } else {
        int integer = compute_integer_part(bar_metrics.served_groups, total_groups);
        int decimal = compute_decimal_part(bar_metrics.served_groups, total_groups);
        seq_printf(m, "Rating: %d.%03d\n", integer, decimal);
    }

    mutex_unlock(&metrics_mutex);
   // printk(KERN_INFO "metrics_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

    return 0;

}

static int bar_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, bar_proc_show, NULL);
}

static const struct proc_ops bar_proc_ops =
{
    .proc_open = bar_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
long open_bar(void)
{
    int ret = 0;
    printk(KERN_INFO "open_bar: Start\n");

    mutex_lock(&bar_state_mutex);
    printk(KERN_INFO "bar_state_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

    if (bar_is_open) {
        printk(KERN_INFO "Bar already open\n");
        mutex_unlock(&bar_state_mutex);
        printk(KERN_INFO "bar_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
        return 1; // Special case per project requirements
    }

    // Initialize tables
    init_tables(&bar_tables);


    // Start servers
    ret = init_servers();
    if (ret) {
        printk(KERN_ERR "Server initialization failed: %d\n", ret);

        mutex_unlock(&bar_state_mutex);
        return ret; // Returns specific error code
    }

    bar_is_open = 1;
    mutex_unlock(&bar_state_mutex);
    printk(KERN_INFO "bar_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    return 0; // Success
}


// Implementation of bar_group_arrive system call
long bar_group_arrive(int id, int num_customers, int stay_duration,
                int spending, int waiting_time)
{
     printk(KERN_INFO "Group %d arrived: %d customers, %d sec stay, spending $%d\n",
           id, num_customers, stay_duration, spending);

    // Prevent new groups if the bar is closed
    mutex_lock(&bar_state_mutex);
    printk(KERN_INFO "bar_state_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    if (bar_is_open == 0) {
        printk(KERN_ERR "ERROR: Bar is closed! Group %d cannot enter.\n", id);
        mutex_unlock(&bar_state_mutex);
        return -EBUSY;  // Return an error
    }
    mutex_unlock(&bar_state_mutex);
    printk(KERN_INFO "bar_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

    if (num_customers < 1 || num_customers > 8) return -EINVAL;
    if (stay_duration < 1 || stay_duration > 100) return -EINVAL;
    if (spending < 1 || spending > 100) return -EINVAL;
    if (waiting_time < 1 || waiting_time > 100) return -EINVAL;

    // Create group
    struct group *g = create_group(id, num_customers, stay_duration, spending, waiting_time);
    if (!g) return -ENOMEM; // Handle allocation failure

    // Assign parameters (modify create_group to accept these or set them here)
    g->customers = num_customers;
    g->stay_duration = stay_duration;
    g->spending = spending;
    g->waiting_time = waiting_time;

    // Add to waiting list
    printk(KERN_INFO "Adding to waiting list...\n");
    mutex_lock(&waiting_list_mutex);
    printk(KERN_INFO "waiting_list_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    //printk(KERN_INFO "waiting_list_mutex locked before calling add group function\n");
    add_group(&waiting_groups, g);
    mutex_unlock(&waiting_list_mutex);
    printk(KERN_INFO "waiting_list_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    //printk(KERN_INFO "waiting_list_mutex unlocked after calling add group function\n");

    printk(KERN_INFO "Group %d added to waiting list\n", id);
    return 0;
}
// Implementation of close_bar system call
long close_bar(void)
{
    printk(KERN_INFO "close_bar invoked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

    //printk(KERN_INFO "SYSCALL_DEFINE0 close_bar called\n");

    mutex_lock(&bar_state_mutex);
    printk(KERN_INFO "bar_state_mutex acquired by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
;
    if (!bar_is_open) {
        mutex_unlock(&bar_state_mutex);
        printk(KERN_INFO "bar_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
        printk(KERN_INFO "bar_close_bar_syscall: Bar was already closed\n");
        return -EINVAL; // Bar not open
    }
    bar_is_open = 0;
    mutex_unlock(&bar_state_mutex);
    printk(KERN_INFO "bar_state_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    //printk(KERN_INFO "3 Setting bar_is_open to %d\n", bar_is_open);

    printk(KERN_INFO "bar_close_bar_syscall: Bar set to closed\n");
      cleanup_servers();

    printk(KERN_INFO "Cleaning groups from waiting list\n");
    cleanup_groups(&waiting_groups);

    //mutex_unlock(&bar_state_mutex);
    printk(KERN_INFO "Bar closed\n");
    return 0;
}

// Module initialization function
static int __init bar_module_init(void)
{

    printk(KERN_INFO "Bar Module: initializing\n");

    // Initialize metrics
    init_metrics(&bar_metrics);

    init_waiting_list(&waiting_groups); // Initialize the waiting list

    // Initialize mutexes
    initialize_mutexes();
    printk(KERN_INFO "mutexes initialized\n");

    // Initialize tables
    init_tables(&bar_tables);


   // Create proc file
   bar_proc_entry = proc_create("bar_status", 0, NULL, &bar_proc_ops);
   if (!bar_proc_entry)
   {
       printk(KERN_ERR "Bar Module: Failed to create proc entry\n");
       mutex_destroy(&bar_state_mutex);
       mutex_destroy(&waiting_list_mutex);
       return -ENOMEM;
   }

     // Assign syscall implementations to STUB pointers
   STUB_open_bar = open_bar;
   STUB_bar_group_arrive = bar_group_arrive;
   STUB_close_bar = close_bar;



   printk(KERN_INFO "Bar Module: initialized : successfully\n");
   return 0;
}

// Module cleanup function
static void __exit bar_module_exit(void)
{
   printk(KERN_INFO "Bar Module: exiting\n");

   // stop the server thread
  // stop_server_thread(&s);

   // Reset STUB pointers to NULL
   STUB_open_bar = NULL;
   STUB_bar_group_arrive = NULL;
   STUB_close_bar = NULL;

   // Remove proc file
   if (bar_proc_entry)
   {
       proc_remove(bar_proc_entry);
   }

   // Clean up mutexes
   destroy_mutexes();

   printk(KERN_INFO "Bar Module: exited successfully\n");
      //return 0;
}
module_init(bar_module_init);
module_exit(bar_module_exit);
