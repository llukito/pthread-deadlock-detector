// Deadlock should be detected
#include <pthread.h>
#include <unistd.h>

#define NUM 10
pthread_mutex_t locks[NUM];

void* worker(void* arg) {
    long id = (long)arg;
    pthread_mutex_lock(&locks[id]);
    sleep(1);
    pthread_mutex_lock(&locks[(id + 1) % NUM]); // The last thread closes the circle
    return NULL;
}

int main() {
    pthread_t threads[NUM];
    for(int i=0; i<NUM; i++) pthread_mutex_init(&locks[i], NULL);
    for(long i=0; i<NUM; i++) pthread_create(&threads[i], NULL, worker, (void*)i);
    for(int i=0; i<NUM; i++) pthread_join(threads[i], NULL);
    return 0;
}