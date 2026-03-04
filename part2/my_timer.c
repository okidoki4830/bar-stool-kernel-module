// my_timer.c - A Linux kernel module for tracking time via /proc/timer

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/mutex.h>

static struct timespec64 last_time;
static struct mutex time_lock;

// Function to read the timer status
static int timer_proc_show(struct seq_file *m, void *v)
{
    struct timespec64 current_time, elapsed;

    // Get the current time
    ktime_get_real_ts64(&current_time);

    mutex_lock(&time_lock);
    elapsed.tv_sec = current_time.tv_sec - last_time.tv_sec;
    elapsed.tv_nsec = current_time.tv_nsec - last_time.tv_nsec;
    last_time = current_time; // Update last recorded time
    mutex_unlock(&time_lock);

    // Print current time
    seq_printf(m, "current time: %lld.%09ld\n", (long long)current_time.tv_sec, current_time.tv_nsec);
    
    // Print elapsed time if valid
    if (elapsed.tv_sec > 0 || elapsed.tv_nsec > 0)
    {
        seq_printf(m, "elapsed time: %lld.%09ld\n", (long long)elapsed.tv_sec, elapsed.tv_nsec);
    }

    return 0;
}

// Proc file operations
static int timer_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, timer_proc_show, NULL);
}

static const struct proc_ops timer_proc_ops =
{
    .proc_open = timer_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

// Module initialization
static int __init my_timer_init(void)
{
    mutex_init(&time_lock);
    ktime_get_real_ts64(&last_time); // Store initial time
    proc_create("timer", 0, NULL, &timer_proc_ops);
    printk(KERN_INFO "my_timer module loaded\n");
    return 0;
}

// Module cleanup
static void __exit my_timer_exit(void)
{
    remove_proc_entry("timer", NULL);
    printk(KERN_INFO "my_timer module unloaded\n");
}

module_init(my_timer_init);
module_exit(my_timer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple kernel timer module");
