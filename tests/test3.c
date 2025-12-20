// Triple-Deadlock should be detected
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER, B = PTHREAD_MUTEX_INITIALIZER, C = PTHREAD_MUTEX_INITIALIZER;

void* t1(void* arg) { pthread_mutex_lock(&A); sleep(1); pthread_mutex_lock(&B); return NULL; }
void* t2(void* arg) { pthread_mutex_lock(&B); sleep(1); pthread_mutex_lock(&C); return NULL; }
void* t3(void* arg) { pthread_mutex_lock(&C); sleep(1); pthread_mutex_lock(&A); return NULL; }

int main() {
    pthread_t p1, p2, p3;
    pthread_create(&p1, NULL, t1, NULL); pthread_create(&p2, NULL, t2, NULL); pthread_create(&p3, NULL, t3, NULL);
    pthread_join(p1, NULL); pthread_join(p2, NULL); pthread_join(p3, NULL);
    return 0;
}