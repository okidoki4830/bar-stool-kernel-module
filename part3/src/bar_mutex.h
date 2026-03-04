#ifndef BAR_MUTEX_H
#define BAR_MUTEX_H

#include <linux/mutex.h>

extern struct mutex bar_state_mutex;
extern struct mutex waiting_list_mutex;
extern struct mutex table_mutex[];
extern struct mutex metrics_mutex;
extern struct mutex table_manager_mutex;
extern struct mutex seated_groups_mutex;
// init and cleanup functions
void initialize_mutexes(void);
void destroy_mutexes(void);

#endif
