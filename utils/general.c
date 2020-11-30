//
// Created by wjx on 2020/11/11.
//

#include "general.h"

#include <stdio.h>
#include <sys/time.h>
#include <stddef.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/dir.h>

unsigned long getCurrentTimeMilli() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

char *pidListToCommaSeparatedString(pid_t *pidList, int pidListLen) {
    // 一个pid预留10个字节
    char *buf = malloc(10 * pidListLen * sizeof(char));
    char *start = buf;
    for (int i = 0; i < pidListLen; i++) {
        start += sprintf(start, "%d", pidList[i]);
        *start++ = ',';
    }
    *(start - 1) = '\0';
    return buf;
}

char *joinString(char **str, int lenStr, char sep) {
    size_t totalLen = 0;
    for (int i = 0; i < lenStr; i++) {
        totalLen += strlen(str[i]) + 1;
    }
    char *res = malloc(totalLen);
    char *curr = res;
    for (int i = 0; i < lenStr; i++) {
        size_t len = strlen(str[i]);
        memcpy(curr, str[i], len);
        curr[len] = sep;
        curr += len + 1;
    }
    *(curr - 1) = 0;
    return res;
}

inline int highestBit(int n) {
    n |= (n >> 1);
    n |= (n >> 2);
    n |= (n >> 4);
    n |= (n >> 8);
    n |= (n >> 16);
    return n - (n >> 1);
}

inline int processRunning(pid_t pid) {
    if (kill(pid, 0) == -1) {
        return -1;
    }
    char fileName[100];
    sprintf(fileName, "/proc/%d/stat", pid);
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        return errno;
    }
    char status;
    fscanf(file, "%*d %*s %s %*d", &status);
    switch (status) {
        case 'S':
        case 'D':
        case 'R':
            return 0;
        default:
            return -2;
    }
}

int recursivelyRemove(const char *path) {
    struct stat s;
    stat(path, &s);
    if (S_ISDIR(s.st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }
            char childPath[PATH_MAX];
            sprintf(childPath, "%s/%s", path, ent->d_name);
            recursivelyRemove(childPath);
        }
        free(dir);
    }
    if (-1 == remove(path)) {
        printf("删除%s出错: %s", path, strerror(errno));
        return -1;
    }
    return 0;
}