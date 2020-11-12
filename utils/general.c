//
// Created by wjx on 2020/11/11.
//

#include "general.h"

#include <sys/time.h>
#include <stddef.h>

unsigned long getCurrentTimeMilli() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
