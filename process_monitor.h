//
// Created by wjx on 2020/11/10.
//

#ifndef RESOURCEMANAGER_PROCESS_MONITOR_H
#define RESOURCEMANAGER_PROCESS_MONITOR_H

#include <pthread.h>
#include <stdbool.h>

struct ProcessMonitor;

int monitorCreate(struct ProcessMonitor *ctx, unsigned int sleepMilli);

int monitorDestroy(struct ProcessMonitor *ctx);

int addProcess(struct ProcessMonitor *ctx, pid_t pid);

int removeProcess(struct ProcessMonitor *ctx, pid_t pid);

#endif //RESOURCEMANAGER_PROCESS_MONITOR_H
