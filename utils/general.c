//
// Created by wjx on 2020/11/11.
//

#include "general.h"

#include <stdio.h>
#include <sys/time.h>
#include <stddef.h>
#include <string.h>

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
