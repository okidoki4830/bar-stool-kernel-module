obj-m := bar.o
bar-objs := bar_module.o bar_mutex.o groups.o metrics.o tables.o servers.o

all:
        make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
        make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

test:
        gcc -o test_syscalls test_syscalls.c
