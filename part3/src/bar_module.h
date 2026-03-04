#ifndef BAR_MODULE_H
#define BAR_MODULE_H

// System call numbers
//#define SYS_OPEN_BAR 548
//#define SYS_BAR_GROUP_ARRIVE 549
//#define SYS_CLOSE_BAR 550

// Constant for number of tables
#define NUM_TABLES 4


extern long (*STUB_open_bar)(void);
extern long (*STUB_bar_group_arrive)(int, int, int, int, int);
extern long (*STUB_close_bar)(void);
extern int bar_is_open;
extern struct table_manager bar_tables;

// Function prototypes
long open_bar(void);
long bar_group_arrive(int id, int num_customers, int stay_duration, int spending, int waiting_time);
long close_bar(void);


#endif /* BAR_MODULE_H */
