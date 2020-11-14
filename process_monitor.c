//
// Created by wjx on 2020/11/10.
//

#include "process_monitor.h"
#include "utils/general.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pqos.h>
#include <errno.h>
#include <signal.h>

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({      \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

struct ProcessMonitorContext {
    pid_t pid; // 被监控的进程Id
    FILE *outFile; // 输出csv记录的文件
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
    struct pqos_mon_data **groups;
    unsigned int lenGroups;
    unsigned int maxRMID;
};

/**
 * 将record写入到file中
 * @return 成功的话返回大于0的数，失败返回小于0的数。
 */
int writeRecord(struct ProcessMonitorRecord *record, FILE *file) {
    return fprintf(file, "%d,%ld,%ld,%ld,%ld,%ld,%f\n", record->pid, record->timestamp, record->localMemoryDelta,
                   record->remoteMemoryDelta, record->llc, record->llcMisses, record->ipc);
}

/**
 * 回收mon_data资源
 * @param ctx 必须是ProcessMonitorContext中的group成员，否则结果是不可知的
 */
void destroyContext(struct pqos_mon_data *ctx) {
    struct ProcessMonitorContext *monitorCtx = container_of(ctx, struct ProcessMonitorContext, group);
    fclose(monitorCtx->outFile);
    pqos_mon_stop(&monitorCtx->group);
    free(monitorCtx);
}

void *monitorThread(void *args) {
    struct ProcessMonitor *ctx = args;
    struct timespec sleepTime = {
            .tv_sec = ctx->sleepMilli / 1000,
            .tv_nsec = ctx->sleepMilli % 1000 * 1000000
    };

    while (ctx->running) {
        pthread_mutex_lock(&ctx->lock);

        for (int i = 0; i < ctx->lenGroups; i++) {
            pqos_mon_poll(ctx->groups, ctx->lenGroups);

            for (int j = 0; j < ctx->lenGroups; j++) {
                struct ProcessMonitorContext *monitorCtx = container_of(ctx->groups[j], struct ProcessMonitorContext,
                                                                        group);
                struct pqos_mon_data *data = ctx->groups[j];
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

        pthread_mutex_unlock(&ctx->lock);
        nanosleep(&sleepTime, NULL);
    }

    return NULL;
}

struct ProcessMonitor *monitorCreate(unsigned int sleepMilli) {
    const struct pqos_cap *cap;
    const struct pqos_cpuinfo *cpu;
    if (PQOS_RETVAL_OK != pqos_cap_get(&cap, &cpu)) {
        return NULL;
    }

    struct ProcessMonitor *ctx = malloc(sizeof(struct ProcessMonitor));
    ctx->sleepMilli = sleepMilli;
    ctx->running = true;
    pthread_mutex_init(&ctx->lock, NULL);
    ctx->groups = malloc(0);
    ctx->lenGroups = 0;
    ctx->maxRMID = cap->capabilities->u.mon->max_rmid;
    pthread_create(&ctx->tid, NULL, monitorThread, ctx);
    return ctx;
}

int monitorDestroy(struct ProcessMonitor *ctx) {
    ctx->running = false;

    // 等待结束
    pthread_join(ctx->tid, NULL);

    pthread_mutex_lock(&ctx->lock);

    // 清理资源
    for (int i = 0; i < ctx->lenGroups; i++) {
        destroyContext(ctx->groups[i]);
    }

    free(ctx->groups);
    ctx->lenGroups = 0;

    pthread_mutex_unlock(&ctx->lock);
    pthread_mutex_destroy(&ctx->lock);

    free(ctx);
    return 0;
}

int monitorAddProcess(struct ProcessMonitor *ctx, pid_t pid) {
    // 检查进程是否存在
    if (kill(pid, 0) == -1) {
        return errno;
    }

    int retVal = 0;
    pthread_mutex_lock(&ctx->lock);
    // 监控数量不能大于RMID数量
    if (ctx->lenGroups >= ctx->maxRMID) {
        retVal = ERR_MONITOR_FULL;
        goto unlock;
    }

    // 检查是否有重复的
    for (int i = 0; i < ctx->lenGroups; i++) {
        if (ctx->groups[i]->pids[0] == pid) {
            retVal = ERR_DUPLICATE_PID;
            goto unlock;
        }
    }

    char filename[50];
    sprintf(filename, "%d.csv", pid);
    FILE *outFile = fopen(filename, "w");
    if (outFile == NULL) {
        retVal = errno;
        goto unlock;
    }

    struct ProcessMonitorContext *monitorCtx = malloc(sizeof(struct ProcessMonitorContext));
    memset(monitorCtx, 0, sizeof(struct ProcessMonitorContext));
    monitorCtx->pid = pid;
    monitorCtx->outFile = outFile;
    pqos_mon_start_pid(pid,
                       PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                       PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS, NULL, &monitorCtx->group);

    ctx->groups = realloc(ctx->groups, (ctx->lenGroups + 1) * sizeof(struct ProcessMonitorContext *));
    ctx->groups[ctx->lenGroups++] = &monitorCtx->group;

    unlock:
    pthread_mutex_unlock(&ctx->lock);
    return retVal;
}

int monitorRemoveProcess(struct ProcessMonitor *ctx, pid_t pid) {
    pthread_mutex_lock(&ctx->lock);
    for (int i = 0; i < ctx->lenGroups; i++) {
        if (ctx->groups[i]->pids[0] == pid) {
            destroyContext(ctx->groups[i]);
            // 将最后的一个过来
            ctx->groups[i] = ctx->groups[--ctx->lenGroups];
            pthread_mutex_unlock(&ctx->lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&ctx->lock);
    return ESRCH;
}

unsigned int monitorGetMaxProcess(struct ProcessMonitor *ctx) {
    return ctx->maxRMID;
}

