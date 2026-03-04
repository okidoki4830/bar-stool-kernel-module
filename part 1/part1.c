// part1.c - A program to demonstrate system call tracing

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

int main()
{
    // Print a simple message
    printf("Executing system calls in part1.c\n");

    // First system call: get process ID
    pid_t pid = syscall(SYS_getpid);
    printf("Process ID: %d\n", pid);

    // Second system call: get parent process ID
    pid_t ppid = syscall(SYS_getppid);
    printf("Parent Process ID: %d\n", ppid);

    // Third system call: get current user ID
    uid_t uid = syscall(SYS_getuid);
    printf("User ID: %d\n", uid);

    // Fourth system call: get system uptime (dummy syscall)
    syscall(SYS_gettimeofday, NULL, NULL);

    return 0;
}
