#include "graph.h"
#include <stddef.h>

/* DFS helper for boolean detection (keeps previous behavior) */
static int dfs_bool(wait_for_graph_t *graph, size_t index, int visited[], int rec_stack[]) {
    visited[index] = 1;
    rec_stack[index] = 1;

    wait_for_node_t *node = &graph->nodes[index];
    for (size_t i = 0; i < node->count; ++i) {
        pthread_t target_tid = node->waiting_for[i];
        size_t target_index = 0;
        int found = 0;
        for (size_t j = 0; j < graph->node_count; ++j) {
            if (graph->nodes[j].tid == target_tid) {
                target_index = j;
                found = 1;
                break;
            }
        }
        if (!found) continue;

        if (!visited[target_index] && dfs_bool(graph, target_index, visited, rec_stack)) {
            return 1;
        } else if (rec_stack[target_index]) {
            return 1; /* cycle detected */
        }
    }

    rec_stack[index] = 0;
    return 0;
}

/* main boolean cycle detection */
int detect_deadlock(wait_for_graph_t *graph) {
    int visited[MAX_THREADS] = {0};
    int rec_stack[MAX_THREADS] = {0};

    for (size_t i = 0; i < graph->node_count; ++i) {
        if (!visited[i] && dfs_bool(graph, i, visited, rec_stack)) {
            return 1; /* deadlock detected */
        }
    }
    return 0;
}

/* ----------------- cycle extraction ----------------- */

/* DFS that records path; if it finds a back edge into the recursion stack,
   it copies the cycle (from where the target index appears in path to current depth)
   into the provided 'cycle' array and sets *cycle_len. Returns 1 if a cycle found. */
static int dfs_find_cycle(wait_for_graph_t *graph,
                          size_t cur_index,
                          int visited[],
                          int rec_stack[],
                          pthread_t path[],
                          size_t depth,
                          pthread_t out_cycle[],
                          size_t *out_len) {
    visited[cur_index] = 1;
    rec_stack[cur_index] = 1;
    path[depth] = graph->nodes[cur_index].tid;

    wait_for_node_t *node = &graph->nodes[cur_index];
    for (size_t i = 0; i < node->count; ++i) {
        pthread_t target_tid = node->waiting_for[i];

        /* find index of target_tid in graph */
        size_t target_index = 0;
        int found = 0;
        for (size_t j = 0; j < graph->node_count; ++j) {
            if (graph->nodes[j].tid == target_tid) {
                target_index = j;
                found = 1;
                break;
            }
        }
        if (!found) continue;

        if (!visited[target_index]) {
            if (dfs_find_cycle(graph, target_index, visited, rec_stack, path, depth + 1, out_cycle, out_len)) {
                return 1;
            }
        } else if (rec_stack[target_index]) {
            /* We found a back edge to target_index. Copy the cycle from path[] starting at
               the position where graph->nodes[target_index].tid first appears, up to depth,
               and return it. */
            pthread_t target_tid_in_path = graph->nodes[target_index].tid;
            size_t start = 0;
            for (size_t k = 0; k <= depth; ++k) {
                if (path[k] == target_tid_in_path) { start = k; break; }
            }
            size_t clen = 0;
            for (size_t k = start; k <= depth; ++k) {
                out_cycle[clen++] = path[k];
                if (clen >= MAX_THREADS) break;
            }
            /* to complete cycle, append the starting tid again (optional) - we will keep it simple */
            *out_len = clen;
            return 1;
        }
    }

    rec_stack[cur_index] = 0;
    return 0;
}

/* Public API: find one cycle and return it */
int detect_deadlock_cycle(wait_for_graph_t *graph, pthread_t *cycle, size_t *cycle_len) {
    int visited[MAX_THREADS] = {0};
    int rec_stack[MAX_THREADS] = {0};
    pthread_t path[MAX_THREADS];

    *cycle_len = 0;
    for (size_t i = 0; i < graph->node_count; ++i) {
        if (!visited[i]) {
            if (dfs_find_cycle(graph, i, visited, rec_stack, path, 0, cycle, cycle_len)) {
                return 1;
            }
        }
    }
    return 0;
}
