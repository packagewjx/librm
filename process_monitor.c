//
// Created by wjx on 2020/11/10.
//

#include "process_monitor.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pqos.h>

#define container_of(ptr, type, member) ({      \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

enum MonitorStatus {
    Pending,
    Monitoring,
    Ending,
};

struct ProcessMonitorContext {
    pid_t pid; // 被监控的进程Id
    FILE *outFile; // 输出csv记录的文件
    enum MonitorStatus status; // 监控状态
    struct pqos_mon_data *group;
};

struct ProcessMonitorRecord {
    pid_t pid;
    unsigned long timestamp;
    unsigned long localMemoryDelta;
    unsigned long remoteMemoryDelta;
    unsigned long llc;
    unsigned long llcMisses;
    double ipc;
};

struct ProcessMonitor {
    pthread_t tid;
    bool running;
    unsigned int sleepMilli;
    pthread_mutex_t lock;
    struct ProcessMonitorContext **monitors;
    unsigned int lenMonitors;
};

/**
 * 将record写入到file中
 * @return 成功的话返回大于0的数，失败返回小于0的数。
 */
int writeRecord(struct ProcessMonitorRecord *record, FILE *file) {
    return fprintf(file, "%d,%ld,%ld,%ld,%ld,%ld,%f", record->pid, record->timestamp, record->localMemoryDelta,
                   record->remoteMemoryDelta, record->llc, record->llcMisses, record->ipc);
}

void *monitorThread(void *args) {
    struct ProcessMonitor *ctx = args;
    struct timespec sleepTime = {
            .tv_sec = ctx->sleepMilli / 1000,
            .tv_nsec = ctx->sleepMilli % 1000 * 1000000
    };

    while (ctx->running) {
        pthread_mutex_lock(&ctx->lock);


        for (int i = 0; i < ctx->lenMonitors; i++) {
            switch (ctx->monitors[i]->status) {
                case Pending: {
                    char filename[50];
                    sprintf(filename, "%d.csv", ctx->monitors[i]->pid);
                    ctx->monitors[i]->outFile = fopen(filename, "w");
                    ctx->monitors[i]->status = Monitoring;
                    break;
                }
                case Monitoring: {

                    break;
                }
                case Ending: {
                    fclose(ctx->monitors[i]->outFile);
                    // 将尾部元素移动到这里以移除
                    ctx->monitors[i] = ctx->monitors[ctx->lenMonitors - 1];
                    ctx->lenMonitors--;
                    i--;
                    break;
                }
            }
        }

        pthread_mutex_unlock(&ctx->lock);
        nanosleep(&sleepTime, NULL);
    }
    return NULL;
}

int monitorCreate(struct ProcessMonitor *ctx, unsigned int sleepMilli) {
    ctx->sleepMilli = sleepMilli;
    ctx->running = true;
    pthread_mutex_init(&ctx->lock, NULL);
    ctx->monitors = malloc(0);
    ctx->lenMonitors = 0;
    return pthread_create(&ctx->tid, NULL, monitorThread, ctx);
}

int monitorDestroy(struct ProcessMonitor *ctx) {
    ctx->running = false;
    free(ctx->monitors);
    ctx->lenMonitors = 0;

    // 清理资源




    return pthread_join(ctx->tid, NULL);
}

