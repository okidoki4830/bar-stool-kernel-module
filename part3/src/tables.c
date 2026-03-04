#include "tables.h"
#include "bar_mutex.h"
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>


void init_tables(struct table_manager *tm)
{
        int i, j;

      for (i = 0; i < NUM_TABLES; i++) {
        for (j = 0; j < STOOLS_PER_TABLE; j++) {
            tm->tables[i].stools[j] = STOOL_FREE;
        }
        tm->tables[i].status = TABLE_NORMAL;
        tm->tables[i].occupied_count = 0;
        tm->tables[i].dirty_count = 0;
    }

}




// Check if a table is empty (no stools in use)
int is_table_empty(struct table_manager *tm, int table_idx)
{
    int ret = 0;

    if (table_idx < 0 || table_idx >= NUM_TABLES)
        return ret;

   // mutex_lock(&table_mutex[table_idx]);
    ret = (tm->tables[table_idx].occupied_count == 0);
    printk(KERN_INFO "DEBUG: Table %d empty check -> %d (occupied_count: %d)\n",
           table_idx, ret, tm->tables[table_idx].occupied_count);
   // mutex_unlock(&table_mutex[table_idx]);


    return ret;
}

// Check if a table has dirty stools
int is_table_dirty(struct table_manager *tm, int table_idx)
{
    int ret = 0;

    if (table_idx < 0 || table_idx >= NUM_TABLES)
        return ret;

   // mutex_lock(&table_mutex[table_idx]);
    ret = (tm->tables[table_idx].dirty_count > 0);
    printk(KERN_INFO "DEBUG: Table %d dirty check -> %d (dirty_count: %d)\n",
           table_idx, ret, tm->tables[table_idx].dirty_count);
   // mutex_unlock(&table_mutex[table_idx]);

        return ret;
}

// Find the required number of adjacent stools
int get_adjacent_stools(struct table_manager *tm, int num_required, int *start_table, int *start_stool)
{
        int i, j, k;
        int consecutive_free = 0;
        int found = 0;

// Check all tables
        for (i = 0; i < NUM_TABLES && !found; i++)
        {
                mutex_lock(&table_mutex[i]);
                // Skip tables being cleaned
                if (tm->tables[i].status == TABLE_CLEANING) {
                        mutex_unlock(&table_mutex[i]);
                        continue;
                }

    // Try starting from each stool in the table
    for (j = 0; j < STOOLS_PER_TABLE && !found; j++)
    {
        consecutive_free = 0;

        // Check for consecutive free stools from current position
        for (k = 0; k < num_required && !found; k++)
        {
            int table_idx = (i + (j + k) / STOOLS_PER_TABLE) % NUM_TABLES;
            int stool_idx = (j + k) % STOOLS_PER_TABLE;

            // If we move to a different table that's being cleaned, break
            if (table_idx != i && tm->tables[table_idx].status == TABLE_CLEANING)
                break;

            // If stool is free and clean, increment consecutive count
            if (tm->tables[table_idx].stools[stool_idx] == STOOL_FREE)
            {
                consecutive_free++;

                // If we found enough consecutive free stools, save starting position
                if (consecutive_free == num_required)
                {
                    *start_table = i;
                    *start_stool = j;
                    found = 1;
                    break;
                }
            }
            else
            {
                // Consecutiveness broken, try from next stool
                break;
            }
        }
         mutex_unlock(&table_mutex[i]);
    }
}

return found;
}

// Allocate stools for a group
int allocate_stools(struct table_manager *tm, int group_id, int num_customers, int start_table, int start_stool)
{
        int i;
        int table_idx, stool_idx;
        int ret = 0;
        int allocated = 0;

        // Validate parameters
        if (group_id <= 0 || num_customers <= 0 || num_customers > TOTAL_STOOLS) {
                ret = -1;
                goto out;
        }

        if (start_table < 0 || start_table >= NUM_TABLES || start_stool < 0 || start_stool >= STOOLS_PER_TABLE) {
                ret = -1;
                goto out;
        }

        // Allocate each stool
        for (i = 0; i < num_customers; i++)
        {
                table_idx = (start_table + (start_stool + i) / STOOLS_PER_TABLE) % NUM_TABLES;
                stool_idx = (start_stool + i) % STOOLS_PER_TABLE;

               // Lock the table with mutex
                mutex_lock(&table_mutex[table_idx]);

                // Double-check if stool is still free and clean
                if (tm->tables[table_idx].stools[stool_idx] != STOOL_FREE)
                {
                // Marked for cleanup later
                        ret = -1;
                        mutex_unlock(&table_mutex[table_idx]);
                        break;
                }

                // Allocate the stool
                tm->tables[table_idx].stools[stool_idx] = group_id;
                tm->tables[table_idx].occupied_count++;
                allocated++;

                mutex_unlock(&table_mutex[table_idx]);
        }

        // If allocation failed, clean up already allocated stools
        if (ret == -1 && allocated > 0)
        {
                for (i = 0; i < allocated; i++)
                {
                        int prev_table = (start_table + (start_stool + i) / STOOLS_PER_TABLE) % NUM_TABLES;
                        int prev_stool = (start_stool + i) % STOOLS_PER_TABLE;

                        mutex_lock(&table_mutex[prev_table]);
                        tm->tables[prev_table].stools[prev_stool] = STOOL_FREE;
                        tm->tables[prev_table].occupied_count--;
                        mutex_unlock(&table_mutex[prev_table]);
                }
        }

        out:
                return ret;
        }

// Free stools when a group leaves
void free_stools(struct table_manager *tm, int group_id)
{

    int i, j;
    if (group_id <= 0)
    return;

// Check all tables and stools
for (i = 0; i < NUM_TABLES; i++)
{
    //mutex_lock(&table_mutex[i]);

    for (j = 0; j < STOOLS_PER_TABLE; j++)
    {
        // Mark stools used by this group as dirty
        if (tm->tables[i].stools[j] == group_id)
        {
            tm->tables[i].stools[j] = STOOL_DIRTY;
            tm->tables[i].occupied_count--;
            tm->tables[i].dirty_count++;
        }
    }

   // mutex_unlock(&table_mutex[i]);
}
}

void cleanup_tables(struct table_manager *tm)
{
    for (int i = 0; i < NUM_TABLES; i++) {
        mutex_lock(&table_mutex[i]);
        // Reset all stools to clean state
        for (int j = 0; j < STOOLS_PER_TABLE; j++) {
            tm->tables[i].stools[j] = STOOL_FREE;
        }
        tm->tables[i].occupied_count = 0;
        tm->tables[i].dirty_count = 0;
        mutex_unlock(&table_mutex[i]);
    }
}


int clean_table(struct table_manager *tm, int table_idx)
{
    int j;
    int ret = 0;

    // Ensure valid table index
    if (table_idx < 0 || table_idx >= NUM_TABLES)
    {
        ret = -1;
        goto out;
    }

    // Check if the table is empty
    if (tm->tables[table_idx].occupied_count > 0)
    {
        ret = -1;  // Cannot clean, table has customers
        goto out;
    }

    // Check if the table is already being cleaned
    if (tm->tables[table_idx].status == TABLE_CLEANING)
    {
        ret = -1;  // Already cleaning
        goto out;
    }
    // Start cleaning
    tm->tables[table_idx].status = TABLE_CLEANING;

    // Mark all dirty stools as clean
    for (j = 0; j < STOOLS_PER_TABLE; j++)
    {
        if (tm->tables[table_idx].stools[j] == STOOL_DIRTY)
        {
            tm->tables[table_idx].stools[j] = STOOL_FREE;
        }
    }

    // Reset dirty stool count
    tm->tables[table_idx].dirty_count = 0;

    // In reality, we would wait for 2 seconds here
    //msleep(2000);

    // Update status after cleaning is complete
    tm->tables[table_idx].status = TABLE_NORMAL;

out:
    return ret;
}


bool find_and_reserve_stools(struct table_manager *tm, int num_customers, int group_id) {
    int i, j, count;

    mutex_lock(&table_manager_mutex);
    printk(KERN_INFO "table_manager_mutex locked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);

    for (i = 0; i < NUM_TABLES; i++) {
        count = 0;

        for (j = 0; j < STOOLS_PER_TABLE; j++) {                        // not reserved
            if (tm->tables[i].stools[j] == STOOL_FREE && tm->tables[i].reserved_group_id[j] == 0) {
                count++;

                if (count == num_customers) {
                    // Reserve stools for the group
                    for (int k = j - num_customers + 1; k <= j; k++) {
                        tm->tables[i].reserved_group_id[k] = group_id; // Mark as reserved
                    }

                    printk(KERN_INFO "Reserved %d stools at Table %d for Group %d\n", num_customers, i, group_id);
                    mutex_unlock(&table_manager_mutex);
                    return true;
                }
            } else {
                count = 0;  // Reset count if a stool is occupied or reserved
            }
        }
    }

    mutex_unlock(&table_manager_mutex);
    printk(KERN_INFO "table_manager_mutex unlocked by: %p, PID: %d, Name: %s\n", current, current->pid, current->comm);
    return false;
}

void get_table_status(struct table_manager *tm, char *buffer, int *len) {
    for (int i = 0; i < NUM_TABLES; i++) {
        mutex_lock(&table_manager_mutex);

        *len += sprintf(buffer + *len, "Table %d: ", i + 1);

        for (int j = 0; j < STOOLS_PER_TABLE; j++) {
            int stool_state = tm->tables[i].stools[j];
            if (stool_state == STOOL_FREE) {
                *len += sprintf(buffer + *len, "[ ] ");
            } else if (stool_state == STOOL_DIRTY) {
                *len += sprintf(buffer + *len, "[x] ");
            } else if (tm->tables[i].reserved_group_id[j] != 0) {
                *len += sprintf(buffer + *len, "[R] ");  // Reserved stool
            } else {
                *len += sprintf(buffer + *len, "[%d] ", stool_state);  // Group ID
            }
        }

        // Add cleaning status
        if (tm->tables[i].status == TABLE_CLEANING) {
            *len += sprintf(buffer + *len, "-- cleaning");
        }

        *len += sprintf(buffer + *len, "\n");

        mutex_unlock(&table_manager_mutex);
   }
}
