#ifndef TABLES_H
#define TABLES_H

#include <linux/types.h>
#include "bar_module.h"

// Stool status definitions
#define STOOL_FREE    0   // Empty and clean stool [ ]
#define STOOL_DIRTY   -1  // Dirty stool [x]
#define STOOL_RESERVED -2  // Reserved but not seated yet
// Positive values represent group IDs (e.g., 1 means Group 1 is using it)

// Table status definitions
#define TABLE_NORMAL    0  // Normal state
#define TABLE_CLEANING  1  // Being cleaned

// Constants
#define STOOLS_PER_TABLE 8
#define TOTAL_STOOLS (NUM_TABLES * STOOLS_PER_TABLE)

// Table structure
struct table
{
    int stools[STOOLS_PER_TABLE];  // Status of each stool (STOOL_FREE, STOOL_DIRTY, or group ID)
    int status;                     // Table status (TABLE_NORMAL, TABLE_CLEANING)
    int occupied_count;             // Number of stools currently in use
    int dirty_count;              // Number of dirty stools
    int reserved_group_id[STOOLS_PER_TABLE];  // Reserved flag for each stool
};

// Table management structure
struct table_manager
{
    struct table tables[NUM_TABLES];
};
extern struct table_manager bar_tables;

// Function prototypes
void init_tables(struct table_manager *tm);
int is_table_empty(struct table_manager *tm, int table_idx);
int is_table_dirty(struct table_manager *tm, int table_idx);
int get_adjacent_stools(struct table_manager *tm, int num_required, int *start_table, int *start_stool);
int allocate_stools(struct table_manager *tm, int group_id, int num_customers, int start_table, int start_stool);
void free_stools(struct table_manager *tm, int group_id);
int clean_table(struct table_manager *tm, int table_idx);
void get_table_status(struct table_manager *tm, char *buffer, int *len);
void cleanup_tables(struct table_manager *tm);
bool has_contiguous_stools(struct table_manager *tm, int required);
bool find_and_reserve_stools(struct table_manager *tm, int num_customers, int group_id);

// ++++++++++++++++
//int is_any_table_dirty(struct table_manager *tm);
#endif /* TABLES_H */
