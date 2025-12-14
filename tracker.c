#define _GNU_SOURCE
#include "tracker.h"
#include "graph.h"   /* provides wait_for_graph_t and wait_for_node_t */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static inline void safe_write(int fd, const void *buf, size_t n) {
    ssize_t _r = write(fd, buf, n);
    (void)_r;  /* explicitly ignore the return value */
}

/* simple spinlock helpers (same as before) */
static inline void spinlock_acq(atomic_flag *f) {
    while (atomic_flag_test_and_set_explicit(f, memory_order_acquire)) {
        /* busy-wait */
    }
}
static inline void spinlock_rel(atomic_flag *f) {
    atomic_flag_clear_explicit(f, memory_order_release);
}

/* helpers to find or create entries */

static mutex_info_t *find_mutex_entry(simple_tracker_t *t, pthread_mutex_t *m) {
    for (size_t i = 0; i < t->mutex_count; ++i) {
        if (t->mutexes[i].mutex == m) return &t->mutexes[i];
    }
    return NULL;
}

static mutex_info_t *get_or_create_mutex_entry(simple_tracker_t *t, pthread_mutex_t *m) {
    mutex_info_t *e = find_mutex_entry(t, m);
    if (e) return e;
    if (t->mutex_count >= MAX_MUTEXES) return NULL;
    e = &t->mutexes[t->mutex_count++];
    e->mutex = m;
    e->owner = (pthread_t)0;
    return e;
}

static thread_info_t *find_thread_entry(simple_tracker_t *t, pthread_t tid) {
    for (size_t i = 0; i < t->thread_count; ++i) {
        if (t->threads[i].tid == tid) return &t->threads[i];
    }
    return NULL;
}

static thread_info_t *get_or_create_thread_entry(simple_tracker_t *t, pthread_t tid) {
    thread_info_t *e = find_thread_entry(t, tid);
    if (e) return e;
    if (t->thread_count >= MAX_THREADS) return NULL;
    e = &t->threads[t->thread_count++];
    e->tid = tid;
    e->waiting = NULL;
    return e;
}

/* API implementations */

void tracker_init(simple_tracker_t *t) {
    t->mutex_count = 0;
    t->thread_count = 0;
    atomic_flag_clear_explicit(&t->lock, memory_order_release);
}

void tracker_destroy(simple_tracker_t *t) {
    /* nothing to free in this simple implementation */
    (void)t;
}

/* Called when tid successfully acquired m */
void tracker_lock_acquired(simple_tracker_t *t, pthread_t tid, pthread_mutex_t *m) {
    spinlock_acq(&t->lock);

    mutex_info_t *me = get_or_create_mutex_entry(t, m);
    if (me) me->owner = tid;

    thread_info_t *th = get_or_create_thread_entry(t, tid);
    if (th) th->waiting = NULL;

    spinlock_rel(&t->lock);
}

/* Called when tid releases m */
void tracker_lock_released(simple_tracker_t *t, pthread_t tid, pthread_mutex_t *m) {
    spinlock_acq(&t->lock);

    mutex_info_t *me = find_mutex_entry(t, m);
    if (me) me->owner = (pthread_t)0;

    spinlock_rel(&t->lock);
}

/* Called when tid is blocked waiting for m */
void tracker_waiting(simple_tracker_t *t, pthread_t tid, pthread_mutex_t *m) {
    spinlock_acq(&t->lock);

    thread_info_t *th = get_or_create_thread_entry(t, tid);
    if (th) th->waiting = m;

    spinlock_rel(&t->lock);
}

/* Simple debug print to stderr using write(2) to avoid stdio reentrancy issues */
void tracker_print_state(simple_tracker_t *t) {
    spinlock_acq(&t->lock);

    char buf[256];
    int n = snprintf(buf, sizeof(buf), "=== Tracker State ===\n");
    safe_write(2, buf, n);

    for (size_t i = 0; i < t->mutex_count; ++i) {
        mutex_info_t *me = &t->mutexes[i];
        n = snprintf(buf, sizeof(buf), " mutex %p -> owner %lu\n", (void*)me->mutex, (unsigned long)me->owner);
        safe_write(2, buf, n);
    }
    for (size_t i = 0; i < t->thread_count; ++i) {
        thread_info_t *th = &t->threads[i];
        n = snprintf(buf, sizeof(buf), " thread %lu -> waiting for %p\n", (unsigned long)th->tid, (void*)th->waiting);
        safe_write(2, buf, n);
    }

    spinlock_rel(&t->lock);
}

/* Build wait-for graph snapshot from tracker data.
 * This function acquires the tracker spinlock internally to ensure a consistent snapshot.
 */
void tracker_build_wait_for_graph(simple_tracker_t *t, struct wait_for_graph_t *graph) {
    /* clear graph */
    graph->node_count = 0;

    spinlock_acq(&t->lock);

    /* For each thread that is waiting, find the owner of the mutex and add edge thread -> owner */
    for (size_t i = 0; i < t->thread_count; ++i) {
        thread_info_t *th = &t->threads[i];
        if (!th->waiting) continue;

        /* find owner of the mutex */
        pthread_t owner = (pthread_t)0;
        for (size_t j = 0; j < t->mutex_count; ++j) {
            if (t->mutexes[j].mutex == th->waiting) {
                owner = t->mutexes[j].owner;
                break;
            }
        }
        if (owner == (pthread_t)0) continue; /* mutex is free or no owner */

        /* get or create node for this thread */
        wait_for_node_t *node = NULL;
        size_t idx = 0;
        for (; idx < graph->node_count; ++idx) {
            if (graph->nodes[idx].tid == th->tid) {
                node = &graph->nodes[idx];
                break;
            }
        }
        if (!node) {
            if (graph->node_count >= MAX_THREADS) {
                /* graph full, skip */
                continue;
            }
            node = &graph->nodes[graph->node_count++];
            node->tid = th->tid;
            node->count = 0;
        }

        /* add owner as a neighbor if not already present */
        if (node->count < MAX_THREADS) {
            node->waiting_for[node->count++] = owner;
        }
    }

    spinlock_rel(&t->lock);
}
