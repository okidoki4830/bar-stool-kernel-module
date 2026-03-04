#ifndef GROUPS_H
#define GROUPS_H

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/jiffies.h>  // not sure if we should use jiffies or ktime.h for kernel time
#include "tables.h"


// REQUIRED INFORATION
// - ID: A unique integer identifying the group ( id >= 1).
// - Customers: The number of people in the group (1–8).
// - Stay Duration: How long, in seconds, the group remains in the bar (1-100).
// - Spending: The monetary amount of dollars earned by the bar (1–100).
// - Waiting Time: The maximum time, in seconds, (1–100), the group will wait for seats. 
//                 if no suitable seats become available within this time, the group leaves.

// REQUIRED CAPABILITIES
// - Waiting list management 
// - Group expiration checking 
// - Group lifecycle handling (arrival/departure)
// - Metrics tracking (serviced/left groups)

struct group
{
    int id;
    int customers;
    int stay_duration;
    int spending;
    int waiting_time;
    bool is_processed;
    unsigned long arrival_time; // keep track of when they got there
    struct list_head list;
};
//struct table_manager;

// Full definition of seated_group
struct seated_group {
    struct list_head list;     // Linked list entry
    struct group *grp;         // Pointer to the group
    unsigned long expiration;  // Expiration time (jiffies)
};

struct waiting_list
{
    struct list_head head;
    int total_serviced;
    int total_left;
};
// Declare the global waiting list
extern struct waiting_list waiting_groups;
void init_waiting_list(struct waiting_list *wl);
struct group *create_group(int id, int num_customers, int stay_duration, int spending, int waiting_time);
void add_group(struct waiting_list *wl, struct group *g);
void remove_group(struct waiting_list *wl, struct group *g, int serviced);
void check_expired_groups(struct waiting_list *wl);
void cleanup_groups(struct waiting_list *wl);
void seat_group(struct table_manager *tm, int group_id);
 void check_expired_seated_groups(struct table_manager *tm, struct list_head *seated_groups);

struct group *select_group_from_waiting_list(struct waiting_list *wl,  struct table_manager *tm);/**

 * select_group_from_waiting_list - Selects a group from the waiting list based on priority
 * @wl: Pointer to the waiting list
 * @available_stools: Number of available stools that can be allocated
 *
 * Return: Selected group or NULL if no suitable group found
 */
//struct group *select_group_from_waiting_list(struct waiting_list *wl, int available_stools);

/**
 * is_group_expired - Checks if a group has exceeded its maximum waiting time
 * @g: Pointer to the group to check
 *
 * Return: 1 if expired, 0 otherwise
 */
int is_group_expired(struct group *g);

/**
 * get_waiting_list_status - Generates a status report of the waiting list
 * @wl: Pointer to the waiting list
 * @buffer: Buffer to store the status report
 * @len: Pointer to the current length of the buffer
 */
void get_waiting_list_status(struct waiting_list *wl, char *buffer, int *len);

#endif

