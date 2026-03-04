#include <linux/kernel.h>
#include <linux/syscalls.h>
#include "bar_module.h"

// Open Bar System Call
SYSCALL_DEFINE0(open_bar)
{
    printk(KERN_INFO "Bar is now open!\n");
    return 0;
}

// Handle Arrived Group System Call
SYSCALL_DEFINE5(bar_group_arrive, int, id, int, num_customers, int, stay_duration, int, spending, int, waiting_time)
{
    printk(KERN_INFO "Group %d arrived: %d customers, %d sec stay, spending $%d\n", 
           id, num_customers, stay_duration, spending);
    return 0;
}

// Close Bar System Call
SYSCALL_DEFINE0(close_bar)
{
    printk(KERN_INFO "Bar is closing...\n");
    return 0;
}
