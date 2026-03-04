#ifndef SERVERS_H
#define SERVERS_H

#include <linux/types.h> // For kernel-specific types if needed

#define NUM_SERVERS 2 
typedef enum {
    SERVER_IDLE,
    SERVER_GUIDING,
    SERVER_CLEANING
} server_state;


struct server {
    struct task_struct *thread;
    int id;
    server_state state;
    int current_group_id;  // Track group being guided
    int current_table_id;  // Track table being cleaned
    struct table_manager *tm;  // Pointer to the shared table manager
};

int server_loop(void *data);
// Server management functions
int init_servers(void);
//int start_server_thread(struct server *s);
//void stop_server_thread(struct server *s);
void cleanup_servers(void);
void get_server_statuses(char *buffer, int *len);

#endif /* SERVERS_H */
