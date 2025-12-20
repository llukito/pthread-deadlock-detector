// should pass
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

void* t1(void* arg) {
    pthread_mutex_lock(&A);
    printf("T1: Locked A, sleeping...\n");
    sleep(2);
    printf("T1: Trying to lock B...\n");
    pthread_mutex_lock(&B);
    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

void* t2(void* arg) {
    sleep(1);
    pthread_mutex_lock(&B);
    printf("T2: Locked B, checking A with trylock...\n");
    if (pthread_mutex_trylock(&A) == EBUSY) {
        printf("T2: Trylock failed (A is busy), but I am NOT blocked. Releasing B to avoid deadlock.\n");
        pthread_mutex_unlock(&B);
    } else {
        printf("T2: Surprisingly got A! Unlocking both.\n");
        pthread_mutex_unlock(&A);
        pthread_mutex_unlock(&B);
    }
    return NULL;
}

int main() {
    pthread_t p1, p2;

    printf("Starting Test 5 (Trylock non-blocking test)...\n");

    pthread_create(&p1, NULL, t1, NULL);
    pthread_create(&p2, NULL, t2, NULL);

    pthread_join(p1, NULL);
    pthread_join(p2, NULL);

    printf("Test 5 finished successfully (Should see NO deadlock reported).\n");
    return 0;
}