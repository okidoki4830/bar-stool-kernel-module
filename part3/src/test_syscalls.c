#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_open_bar 548
#define SYS_bar_group_arrive 549
#define SYS_close_bar 550

int main()
{
    long result;
    
    printf("Testing open_bar syscall (548)...\n");
    result = syscall(SYS_open_bar);
    printf("Result: %ld\n\n", result);
    
    printf("Testing bar_group_arrive syscall (549)...\n");
    result = syscall(SYS_bar_group_arrive, 1, 4, 10, 20, 30);
    printf("Result: %ld\n\n", result);
    
    printf("Testing close_bar syscall (550)...\n");
    result = syscall(SYS_close_bar);
    printf("Result: %ld\n\n", result);
    
    return 0;
}
