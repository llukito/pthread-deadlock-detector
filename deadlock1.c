#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

void *t1(void *_) {
    pthread_mutex_lock(&A);
    fprintf(stderr, "T1: locked A\n");
    sleep(1);
    fprintf(stderr, "T1: try lock B\n");
    pthread_mutex_lock(&B);
    fprintf(stderr, "T1: locked B\n");
    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

void *t2(void *_) {
    pthread_mutex_lock(&B);
    fprintf(stderr, "T2: locked B\n");
    sleep(1);
    fprintf(stderr, "T2: try lock A\n");
    pthread_mutex_lock(&A);
    fprintf(stderr, "T2: locked A\n");
    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    return NULL;
}

int main(void) {
    pthread_t p1, p2;
    pthread_create(&p1, NULL, t1, NULL);
    pthread_create(&p2, NULL, t2, NULL);
    pthread_join(p1, NULL);
    pthread_join(p2, NULL);
    fprintf(stderr, "Finished\n");
    return 0;
}
