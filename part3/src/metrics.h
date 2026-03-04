#ifndef METRICS_H
#define METRICS_H

#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include "groups.h"

// How many decimals
#define PRECISION 1000

struct metrics {
    unsigned long total_profit;
    int served_groups;
    int served_customers;
    int left_groups;                // Incremented when groups have left
};

extern struct metrics bar_metrics;

void init_metrics(struct metrics *m);
void update_metrics_served(struct metrics *m, struct group *g);
void update_metrics_left(struct metrics *m);
int compute_integer_part(int A, int B);     // leveraged from example_code
int compute_decimal_part(int A, int B);     // leveraged from example_code

#endif
