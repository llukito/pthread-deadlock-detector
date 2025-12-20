// Self-Deadlock should be detected
#include <pthread.h>
#include <stdio.h>

int main() {
    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    printf("Thread locking once...\n");
    pthread_mutex_lock(&m);
    printf("Thread locking again (Self-deadlock)...\n");
    pthread_mutex_lock(&m); 
    return 0;
}