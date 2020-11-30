#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

void* th(void* arg) {
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_lock(&lock);
    pid_t pid = fork();
    if (pid == 0) {
        printf("In child\n");
        exit(0);
    } else {
        sleep(1);
        pthread_mutex_unlock(&lock);
        printf("In main\n");
    }
    return NULL;
}

int main() {
    pthread_t thread;
    pthread_create(&thread, NULL, th, NULL);
    pthread_join(thread, NULL);
}