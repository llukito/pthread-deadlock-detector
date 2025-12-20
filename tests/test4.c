// Should pass
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    pthread_mutex_lock(&m);
    usleep(10000); // Hold for 10ms
    pthread_mutex_unlock(&m);
    return NULL;
}

int main() {
    pthread_t threads[50];
    for(int i=0; i<50; i++) pthread_create(&threads[i], NULL, worker, NULL);
    for(int i=0; i<50; i++) pthread_join(threads[i], NULL);
    return 0;
}