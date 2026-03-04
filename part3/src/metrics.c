#include "metrics.h"

//  Initialize the metrics
void init_metrics(struct metrics *m) {
    m->total_profit = 0;
    m->served_groups = 0;
    m->served_customers = 0;
    m->left_groups = 0;
}

// Update metrics after serving a group
void update_metrics_served(struct metrics *m, struct group *g) {
    m->total_profit += g->spending;
    m->served_groups++;
    m->served_customers += g->customers;
}

// Update metrics after a group leaves
void update_metrics_left(struct metrics *m) {
    m->left_groups++;
}

// Compute the integer part | leveraged from example code
int compute_integer_part(int A, int B) {
    if (A == B)
        return 1;
    else
        return 0;
}

// Compute the decimal part | leveraged from example code
int compute_decimal_part(int A, int B) {
    int remainder;
    int decimal_part;

    if (B == 0)
        return 0;

    remainder = A % B;
    decimal_part = (remainder * PRECISION) / B;

    return decimal_part;
}