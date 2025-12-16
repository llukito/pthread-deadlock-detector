#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t M1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t M2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t M3 = PTHREAD_MUTEX_INITIALIZER;

void* t1(void* arg) {
    pthread_mutex_lock(&M1);
    usleep(100000);
    pthread_mutex_lock(&M2); // Waits for T2
    pthread_mutex_unlock(&M2);
    pthread_mutex_unlock(&M1);
    return NULL;
}

void* t2(void* arg) {
    pthread_mutex_lock(&M2);
    usleep(100000);
    pthread_mutex_lock(&M3); // Waits for T3
    pthread_mutex_unlock(&M3);
    pthread_mutex_unlock(&M2);
    return NULL;
}

void* t3(void* arg) {
    pthread_mutex_lock(&M3);
    usleep(100000);
    pthread_mutex_lock(&M1); // Waits for T1
    pthread_mutex_unlock(&M1);
    pthread_mutex_unlock(&M3);
    return NULL;
}

int main() {
    pthread_t threads[3];
    pthread_create(&threads[0], NULL, t1, NULL);
    pthread_create(&threads[1], NULL, t2, NULL);
    pthread_create(&threads[2], NULL, t3, NULL);
    
    for(int i=0; i<3; i++) pthread_join(threads[i], NULL);
    return 0;
}