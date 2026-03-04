/*
 * bar_mutex.c includes functions for initalizing and destroying all mutexes
 * mutexes are needed for handling the tables, waiting list, profit update,
 * and the bar's open/close state (assuming there could be a situation where
 * customers enter at the same time the bar is closing).
 */

#include "bar_module.h"
#include <linux/mutex.h>
#include "bar_mutex.h"

struct mutex bar_state_mutex;
/*
 * to prevent operations while the bar state is changing
 * should be used in bar_module.c and functions that
 * check bar state
 */
struct mutex waiting_list_mutex;
/*
 * to prevent concurrent modifications to the list
 * should be used in groups.c and servers.c
 */
struct mutex table_mutex[NUM_TABLES];
/*
 * to prevent stool allocation conflicts
 * should be used in tables.c and servers.c
 */
struct mutex metrics_mutex;
/*
 * to prevent profit update conflicts
 * should be used in groups.c and bar_status.c
 */

struct mutex seated_groups_mutex;
struct mutex table_manager_mutex;
void initialize_mutexes(void)
{
    mutex_init(&bar_state_mutex);
    mutex_init(&table_manager_mutex);
    mutex_init(&waiting_list_mutex);
    mutex_init(&seated_groups_mutex);
    mutex_init(&metrics_mutex);
    // initialize mutex for each table
    for (int i = 0; i < NUM_TABLES; i++)
        mutex_init(&table_mutex[i]);
}

void destroy_mutexes(void)
{
    mutex_destroy(&bar_state_mutex);
    mutex_destroy(&table_manager_mutex);
    mutex_destroy(&waiting_list_mutex);
    mutex_destroy(&seated_groups_mutex);
    mutex_destroy(&metrics_mutex);
    // destroy mutex for each table
    for (int i = 0; i < NUM_TABLES; i++)
        mutex_destroy(&table_mutex[i]);
}
