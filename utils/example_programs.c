//
// Created by wjx on 2020/12/6.
//

#include "example_programs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MEM_SIZE 0x70000000

void sequenceMemoryAccessor() {
    int *buf = malloc(MEM_SIZE * sizeof(int));
    for (int i = 1; i < MEM_SIZE; i++) {
        buf[i - 1] += buf[i];
    }
    printf("%0d\n", buf[MEM_SIZE - 1]);
    free(buf);
}

void nopLoop() {
    for (int i = 0; i < 0x7FFFFFFF; i++) {}
}

void randomMemoryAccessor() {
    int *buf = malloc(sizeof(int) * MEM_SIZE);
    for (int i = 0; i < 100000000; i++) {
        buf[(int) ((float) random() / RAND_MAX * MEM_SIZE)] += buf[(int) ((float) random() / RAND_MAX * MEM_SIZE)];
    }
    printf("%0d\n", buf[(int) ((float) random() / RAND_MAX * MEM_SIZE)]);
    free(buf);
}

pid_t forkAndRun(void (*program)()) {
    int fd[2];
    pipe(fd);

    pid_t pid = fork();
    if (pid == 0) {
        close(fd[0]);
        write(fd[1], "y", 1);
        close(fd[1]);
        program();
        exit(0);
    }
    close(fd[1]);
    char buf;
    read(fd[0], &buf, 1);
    close(fd[0]);
    return pid;
}





