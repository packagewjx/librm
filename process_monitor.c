//
// Created by wjx on 2020/11/10.
//

#include "process_monitor.h"
#include "utils/pqos_utils.h"
#include "utils/container.h"
#include "utils/general.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pqos.h>
#include <errno.h>

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
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
    struct pqos_mon_data group;
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
    unsigned int numCores;
    unsigned int *cores;
};

/**
 * 将record写入到file中
 * @return 成功的话返回大于0的数，失败返回小于0的数。
 */
int writeRecord(struct ProcessMonitorRecord *record, FILE *file) {
    return fprintf(file, "%d,%ld,%ld,%ld,%ld,%ld,%f\n", record->pid, record->timestamp, record->localMemoryDelta,
                   record->remoteMemoryDelta, record->llc, record->llcMisses, record->ipc);
}

void destroyContext(struct ProcessMonitorContext *ctx) {
    fclose(ctx->outFile);
    pqos_mon_stop(&ctx->group);
    free(ctx);
}

void *monitorThread(void *args) {
    struct ProcessMonitor *ctx = args;
    struct timespec sleepTime = {
            .tv_sec = ctx->sleepMilli / 1000,
            .tv_nsec = ctx->sleepMilli % 1000 * 1000000
    };

    while (ctx->running) {
        unsigned long milli = getCurrentTimeMilli();
        pthread_mutex_lock(&ctx->lock);

        struct Stack monData;
        stackInit(&monData);

        for (int i = 0; i < ctx->lenMonitors; i++) {
            switch (ctx->monitors[i]->status) {
                case Pending: {
                    ctx->monitors[i]->status = Monitoring;
                    pqos_mon_start_pid(ctx->monitors[i]->pid,
                                       PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                                       PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS, NULL, &ctx->monitors[i]->group);
                    break;
                }
                case Monitoring: {
                    stackPush(&monData, &ctx->monitors[i]->group);
                    break;
                }
                case Ending: {
                    destroyContext(ctx->monitors[i]);
                    // 将尾部元素移动到这里以移除
                    ctx->monitors[i] = ctx->monitors[ctx->lenMonitors - 1];
                    ctx->lenMonitors--;
                    i--;
                    break;
                }
            }

            pqos_mon_poll((struct pqos_mon_data **) monData.arr, monData.len);

            for (int j = 0; j < monData.len; j++) {
                struct pqos_mon_data *group = monData.arr[j];
                struct ProcessMonitorContext *monitorCtx = container_of(monData.arr[j], struct ProcessMonitorContext,
                                                                        group);
                struct pqos_mon_data *data = monData.arr[j];
                struct ProcessMonitorRecord record;
                record.pid = monitorCtx->pid;
                record.llc = data->values.llc;
                record.timestamp = getCurrentTimeMilli();
                record.llcMisses = data->values.llc_misses;
                record.ipc = data->values.ipc;
                record.localMemoryDelta = data->values.mbm_local_delta;
                record.remoteMemoryDelta = data->values.mbm_remote_delta;
                writeRecord(&record, monitorCtx->outFile);
            }
        }

        stackDestroy(&monData);
        pthread_mutex_unlock(&ctx->lock);
        nanosleep(&sleepTime, NULL);
    }

    return NULL;
}

struct ProcessMonitor *monitorCreate(unsigned int sleepMilli) {
    const struct pqos_cap *cap;
    const struct pqos_cpuinfo *cpu;
    pqos_cap_get(&cap, &cpu);

    struct ProcessMonitor *ctx = malloc(sizeof(struct ProcessMonitor));
    ctx->sleepMilli = sleepMilli;
    ctx->running = true;
    pthread_mutex_init(&ctx->lock, NULL);
    ctx->monitors = malloc(0);
    ctx->lenMonitors = 0;
    ctx->cores = GetAllCoresId(cpu, &ctx->numCores);
    pthread_create(&ctx->tid, NULL, monitorThread, ctx);
    return ctx;
}

int monitorDestroy(struct ProcessMonitor *ctx) {
    ctx->running = false;

    // 等待结束
    pthread_join(ctx->tid, NULL);

    pthread_mutex_lock(&ctx->lock);

    // 清理资源
    for (int i = 0; i < ctx->lenMonitors; i++) {
        destroyContext(ctx->monitors[i]);
    }

    free(ctx->monitors);
    ctx->lenMonitors = 0;

    pthread_mutex_unlock(&ctx->lock);
    pthread_mutex_destroy(&ctx->lock);

    free(ctx);
    return 0;
}

int monitorAddProcess(struct ProcessMonitor *ctx, pid_t pid) {
    struct ProcessMonitorContext *monitorCtx = malloc(sizeof(struct ProcessMonitorContext));
    monitorCtx->pid = pid;
    char filename[50];
    sprintf(filename, "%d.csv", pid);
    monitorCtx->outFile = fopen(filename, "w");
    if (monitorCtx->outFile == NULL) {
        return errno;
    }
    monitorCtx->status = Pending;

    // FIXME CAS操作更佳
    pthread_mutex_lock(&ctx->lock);
    ctx->monitors = realloc(ctx->monitors, (ctx->lenMonitors + 1) * sizeof(struct ProcessMonitorContext *));
    ctx->monitors[ctx->lenMonitors++] = monitorCtx;
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

int monitorRemoveProcess(struct ProcessMonitor *ctx, pid_t pid) {
    pthread_mutex_lock(&ctx->lock);
    for (int i = 0; i < ctx->lenMonitors; i++) {
        if (ctx->monitors[i]->pid == pid) {
            ctx->monitors[i]->status = Ending;
            pthread_mutex_unlock(&ctx->lock);
            return 0;
        }
    }

    // 找不到也返回0
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

