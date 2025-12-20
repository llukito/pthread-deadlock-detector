#ifndef SIMPLE_TRACKER_H
#define SIMPLE_TRACKER_H

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>

// you can increase these numbers it you are planning to run a big project,
// but remember that you have to then change O(n) access into O(1) with hashmap
#define MAX_MUTEXES 256
#define MAX_THREADS 256

typedef struct {
    pthread_mutex_t *mutex; // key
    pthread_t owner;        // 0 if free
} mutex_info_t;

typedef struct {
    pthread_t tid;            // key
    pthread_mutex_t *waiting; // NULL if not waiting
    void *callstack[10];      // Storage for stack frames (customizible)
    int frames;               // Number of frames captured
} thread_info_t;

typedef struct {
    mutex_info_t mutexes[MAX_MUTEXES];
    size_t mutex_count;

    thread_info_t threads[MAX_THREADS];
    size_t thread_count;

    atomic_flag lock; // spinlock to protect the arrays
} simple_tracker_t;

void tracker_init(simple_tracker_t *t);
void tracker_destroy(simple_tracker_t *t);

// called by interceptors 
void tracker_lock_acquired(simple_tracker_t *t, pthread_t tid, pthread_mutex_t *m);
void tracker_lock_released(simple_tracker_t *t, pthread_t tid, pthread_mutex_t *m);
void tracker_waiting(simple_tracker_t *t, pthread_t tid, pthread_mutex_t *m);

// Debug print current state
void tracker_print_state(simple_tracker_t *t);

/*
 * Build the wait-for graph snapshot from the tracker.
 * This function acquires the tracker's lock internally and
 * fills the provided wait_for_graph_t structure.
 */
struct wait_for_graph_t;
void tracker_build_wait_for_graph(simple_tracker_t *t, struct wait_for_graph_t *graph);

void spinlock_acq(atomic_flag *f);
void spinlock_rel(atomic_flag *f);

#endif
