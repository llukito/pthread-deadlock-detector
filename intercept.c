#define _GNU_SOURCE
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "tracker.h"
#include "graph.h"
#include <execinfo.h>
#include <stdlib.h>
#include <stdint.h>

/* --- Real function pointers --- */
typedef int (*real_lock_t)(pthread_mutex_t *);
typedef int (*real_unlock_t)(pthread_mutex_t *);
typedef int (*real_trylock_t)(pthread_mutex_t *);

static real_lock_t real_pthread_mutex_lock = NULL;
static real_unlock_t real_pthread_mutex_unlock = NULL;
static real_trylock_t real_pthread_mutex_trylock = NULL;

/* --- Global simple tracker --- */
static simple_tracker_t tracker;

/* --- Thread-local guards --- */
static __thread int in_hook = 0;                 // prevents recursion in hooks
static __thread int in_deadlock_detection = 0;  // prevents start of detection while already being done
static __thread int initializing = 0;

/* --- Monitor control --- */
static volatile int monitor_running = 1;
static int deadlock_reported = 0;

// we had to use this to avoid warnings
static inline void safe_write(int fd, const void *buf, size_t n) {
    ssize_t _r = write(fd, buf, n);
    (void)_r; 
}

// we use this helper to resolve stack line with addr2line
void print_resolved_frame(void *addr) {
    // subtract 1 from the address to get the call site instead of return address
    void *call_site = (void *)((uintptr_t)addr - 1);

    // resolve the call_site instead of the raw addr
    char **symbols = backtrace_symbols(&call_site, 1);
    if (!symbols) return;

    char *buf = strdup(symbols[0]);
    char *bin_name = buf;
    char *off_start = strchr(buf, '(');
    char *off_end = off_start ? strchr(off_start, ')') : NULL;

    if (off_start && off_end) {
        *off_start = '\0'; 
        off_start++;       
        *off_end = '\0';   

        char cmd[1024];
        snprintf(cmd, sizeof(cmd), 
                 "LD_PRELOAD= DEADLOCK_QUIET=1 addr2line -f -p -e %s %s 2>/dev/null", 
                 bin_name, off_start);

        FILE *fp = popen(cmd, "r");
        if (fp) {
            char line[512];
            if (fgets(line, sizeof(line), fp) && line[0] != '?') {
                line[strcspn(line, "\n")] = 0;
                safe_write(2, "      -> ", 9);
                safe_write(2, line, strlen(line));
                safe_write(2, "\n", 1);
            } else {
                safe_write(2, "      (raw): ", 13);
                safe_write(2, symbols[0], strlen(symbols[0]));
                safe_write(2, "\n", 1);
            }
            pclose(fp);
        }
    } else {
        safe_write(2, "      (raw): ", 13);
        safe_write(2, symbols[0], strlen(symbols[0]));
        safe_write(2, "\n", 1);
    }
    free(buf);
    free(symbols);
}

// we monitor with this function
static void *monitor_func(void *arg) {
    (void)arg; // again avoids warnings

    in_hook = 1;

    while (monitor_running) {
        // sleep 100ms between checks ( can be customizied )
        usleep(100 * 1000);

        if (deadlock_reported) continue;

        // avoid detecting while already in detection on this thread
        if (in_deadlock_detection) continue;
        in_deadlock_detection = 1;

        // build snapshot of tracker 
        wait_for_graph_t graph;
        tracker_build_wait_for_graph(&tracker, &graph);

        // detect deadlock and extract cycle
        pthread_t cycle[MAX_THREADS];
        size_t cycle_len = 0;
        if (detect_deadlock_cycle(&graph, cycle, &cycle_len)) {
            deadlock_reported = 1;
            safe_write(2, "!!! Deadlock detected !!!\n\nCycle: ", 34);

            char buf[256];
            for (size_t i = 0; i < cycle_len; ++i) {
                int n = snprintf(buf, sizeof(buf), "  T%lu", (unsigned long)cycle[i]);
                safe_write(2, buf, n);
                if (i + 1 < cycle_len) {
                    safe_write(2, " -> ", 4);
                } else {
                    safe_write(2, "\n", 1);
                }
            }
            
            // safe stack printing (Release lock before I/O)
            safe_write(STDERR_FILENO, "\nWait-for Locations:\n\n", 23);
            for (size_t i = 0; i < cycle_len; ++i) {
                void *temp_stack[10];
                int frames = 0;
                pthread_t tid = cycle[i];

                // copy data under lock
                spinlock_acq(&tracker.lock);
                for(size_t j=0; j<tracker.thread_count; j++){
                    if(tracker.threads[j].tid == tid){
                        frames = tracker.threads[j].frames;
                        if(frames > 0) memcpy(temp_stack, tracker.threads[j].callstack, sizeof(void*)*frames);
                        break;
                    }
                }
                spinlock_rel(&tracker.lock);

                // print outside lock
                if (frames > 0) {
                    char h[64];
                    int l = snprintf(h, sizeof(h), "Thread %lu stack:\n", (unsigned long)tid);
                    safe_write(STDERR_FILENO, h, l);
                    for (int k = 0; k < frames; k++) {
                        print_resolved_frame(temp_stack[k]);
                    }
                    safe_write(STDERR_FILENO, "\n", 1);
                }
            }

            // additionally print tracker state for waiting mutex info
            safe_write(2, "Involved waits (tid -> waiting_mutex):\n", 39);
            tracker_print_state(&tracker);

            _exit(1); // no point continuing program, cause its in deadlock
        }

        in_deadlock_detection = 0;
    }

    return NULL;
}

// Constructor: initialize tracker, resolve functions, and start monitor
__attribute__((constructor))
static void deadlock_init(void) {
    if (in_hook) return;
    in_hook = 1;

    if (!getenv("DEADLOCK_QUIET")) {
        safe_write(2, "Deadlock runtime loaded\n", 24);
    }
    
    tracker_init(&tracker);

    real_pthread_mutex_lock =
        (real_lock_t)dlsym(RTLD_NEXT, "pthread_mutex_lock");
    real_pthread_mutex_unlock =
        (real_unlock_t)dlsym(RTLD_NEXT, "pthread_mutex_unlock");
    real_pthread_mutex_trylock =
        (real_trylock_t)dlsym(RTLD_NEXT, "pthread_mutex_trylock");

    if (!real_pthread_mutex_lock || !real_pthread_mutex_unlock || !real_pthread_mutex_trylock) {
        safe_write(2, "ERROR: dlsym failed\n", 20);
    }

    // create detached monitor thread
    pthread_t mid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&mid, &attr, monitor_func, NULL) != 0) {
        safe_write(2, "WARNING: couldn't create monitor thread\n", 39);
    }
    pthread_attr_destroy(&attr);

    in_hook = 0;
}

// Destructor: stop monitor and print final tracker state
__attribute__((destructor))
static void deadlock_fini(void) {
    monitor_running = 0;
    // give monitor a moment to exit if it was sleeping
    usleep(200 * 1000);
    tracker_print_state(&tracker);
    tracker_destroy(&tracker);
}

/* --- Lock interception --- */

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    // lazy initialization if pointer is missing
    if (!real_pthread_mutex_lock) {
        if (initializing) return 0;
        initializing = 1;
        real_lock_t temp = (real_lock_t)dlsym(RTLD_NEXT, "pthread_mutex_lock");
        if (!temp) {
            initializing = 0;
            return 0; 
        }
        real_pthread_mutex_lock = temp;
        initializing = 0;
    }

    if (in_hook) return real_pthread_mutex_lock(mutex);
    in_hook = 1;

    tracker_waiting(&tracker, pthread_self(), mutex);

    int rc = real_pthread_mutex_lock(mutex);
    if (rc == 0) {
        tracker_lock_acquired(&tracker, pthread_self(), mutex);
    } else {
        tracker_waiting(&tracker, pthread_self(), NULL);
    }
 
    in_hook = 0;
    return rc;
}

int __pthread_mutex_lock(pthread_mutex_t *mutex) {
    return pthread_mutex_lock(mutex);
}

/* --- Unlock interception --- */

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (!real_pthread_mutex_unlock) {
        if (initializing) return 0;
        initializing = 1;
        real_unlock_t temp = (real_unlock_t)dlsym(RTLD_NEXT, "pthread_mutex_unlock");
        if (!temp) {
            initializing = 0;
            return 0; 
        }
        real_pthread_mutex_unlock = temp;
        initializing = 0;
    }

    if (in_hook) return real_pthread_mutex_unlock(mutex);
    in_hook = 1;

    int rc = real_pthread_mutex_unlock(mutex);
    if (rc == 0) {
        tracker_lock_released(&tracker, pthread_self(), mutex);
    }

    in_hook = 0;
    return rc;
}

int __pthread_mutex_unlock(pthread_mutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}

/* --- Trylock interception --- */

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    // lazy Initialization
    if (!real_pthread_mutex_trylock) {
        if (initializing) return 0;
        initializing = 1;
        real_trylock_t temp = (real_trylock_t)dlsym(RTLD_NEXT, "pthread_mutex_trylock");
        if (!temp) {
            initializing = 0;
            return 0; 
        }
        real_pthread_mutex_trylock = temp;
        initializing = 0;
    }

    if (in_hook) return real_pthread_mutex_trylock(mutex);
    in_hook = 1;

    int rc = real_pthread_mutex_trylock(mutex);

    if (rc == 0) {
        tracker_lock_acquired(&tracker, pthread_self(), mutex);
    }

    in_hook = 0;
    return rc;
}

int __pthread_mutex_trylock(pthread_mutex_t *mutex) {
    return pthread_mutex_trylock(mutex);
}