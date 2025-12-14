#ifndef WAIT_FOR_GRAPH_H
#define WAIT_FOR_GRAPH_H

#include <pthread.h>
#include <stddef.h>

#define MAX_THREADS 256

typedef struct wait_for_node_t {
    pthread_t tid;
    pthread_t waiting_for[MAX_THREADS];
    size_t count;
} wait_for_node_t;

typedef struct wait_for_graph_t {
    wait_for_node_t nodes[MAX_THREADS];
    size_t node_count;
} wait_for_graph_t;

/* detect deadlock: returns 1 if cycle detected, 0 otherwise */
int detect_deadlock(wait_for_graph_t *graph);

/* find a cycle, fill cycle[] with tids in cycle order, set *cycle_len; returns 1 if found */
int detect_deadlock_cycle(wait_for_graph_t *graph, pthread_t *cycle, size_t *cycle_len);

#endif
