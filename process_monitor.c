//
// Created by wjx on 2020/11/10.
//

#include "resource_manager.h"
#include "utils/general.h"
#include "log/src/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pqos.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

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

int processNotExist(pid_t pid) {
    return kill(pid, 0) == -1;
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

void *monitorThread(void *args) {
    struct ProcessMonitor *ctx = args;
    struct timespec sleepTime = {
            .tv_sec = ctx->sleepMilli / 1000,
            .tv_nsec = ctx->sleepMilli % 1000 * 1000000
    };
    log_info("进程监控线程启动");
    int pollRetry = 0;

    while (ctx->running) {
        pthread_mutex_lock(&ctx->lock);
        if (ctx->lenGroups == 0) {
            log_info("目前没有进程监控，等待下一次唤醒");
            goto unlockAndSleep;
        }

        log_info("正在抓取%d个监控进程的监控数据", ctx->lenGroups);
        int retVal = pqos_mon_poll(ctx->groups, ctx->lenGroups);

        if (retVal != PQOS_RETVAL_OK) {
            log_error("监控抓取失败，返回值为%d");
            pollRetry++;
            if (pollRetry == 5) {
                log_error("多次重试均失败，监控线程退出");
                ctx->running = false;
            }
            goto unlockAndSleep;
        }
        if (pollRetry > 0) {
            pollRetry = 0;
        }

        for (int i = 0; i < ctx->lenGroups; i++) {
            struct ProcessMonitorContext *monitorCtx = container_of(ctx->groups[i], struct ProcessMonitorContext,
                                                                    group);
            struct pqos_mon_data *data = ctx->groups[i];
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

        unlockAndSleep:
        pthread_mutex_unlock(&ctx->lock);
        nanosleep(&sleepTime, NULL);
    }

    log_info("进程监控线程退出");
    return NULL;
}

struct ProcessMonitor *rm_monitor_create(unsigned int sleepMilli) {
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

int rm_monitor_destroy(struct ProcessMonitor *ctx) {
    log_info("接收到结束监控的请求");
    ctx->running = false;

    // 等待结束
    log_info("正在等待监控线程退出");
    pthread_join(ctx->tid, NULL);

    // 清理资源
    log_info("正在写入剩余监控数据并回收资源");
    for (int i = 0; i < ctx->lenGroups; i++) {
        destroyContext(ctx->groups[i]);
    }

    free(ctx->groups);
    ctx->lenGroups = 0;

    pthread_mutex_destroy(&ctx->lock);

    free(ctx);

    log_info("监控成功关闭");
    return 0;
}

int rm_monitor_add_process(struct ProcessMonitor *ctx, pid_t pid) {
    log_info("接收到添加进程的请求，进程pid为%d", pid);
    // 检查进程是否存在
    if (kill(pid, 0) == -1) {
        log_error("进程pid %d 不存在", pid);
        return errno;
    }

    int retVal = 0;
    pthread_mutex_lock(&ctx->lock);
    // 监控数量不能大于RMID数量
    if (ctx->lenGroups >= ctx->maxRMID) {
        log_error("监控进程数已达最大值%d", ctx->lenGroups);
        retVal = ERR_MONITOR_FULL;
        goto unlock;
    }

    // 检查是否有重复的
    for (int i = 0; i < ctx->lenGroups; i++) {
        if (ctx->groups[i]->pids[0] == pid) {
            log_error("已存在pid为%d的监控进程", pid);
            retVal = ERR_DUPLICATE_PID;
            goto unlock;
        }
    }

    char filename[50];
    sprintf(filename, "%d.csv", pid);
    FILE *outFile = fopen(filename, "w");
    if (outFile == NULL) {
        log_error("无法创建文件%s，原因为%s", filename, strerror(errno));
        retVal = errno;
        goto unlock;
    }

    log_info("正在向系统请求加入监控进程%d", pid);
    struct ProcessMonitorContext *monitorCtx = malloc(sizeof(struct ProcessMonitorContext));
    memset(monitorCtx, 0, sizeof(struct ProcessMonitorContext));
    monitorCtx->pid = pid;
    monitorCtx->outFile = outFile;
    retVal = pqos_mon_start_pid(pid,
                                PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                                PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS, NULL, &monitorCtx->group);
    if (retVal != PQOS_RETVAL_OK) {
        log_info("请求监控进程%d失败，返回码为%d", pid, retVal);
        goto unlock;
    }

    ctx->groups = realloc(ctx->groups, (ctx->lenGroups + 1) * sizeof(struct ProcessMonitorContext *));
    ctx->groups[ctx->lenGroups++] = &monitorCtx->group;

    log_info("进程%d已进入监控队列", pid);
    unlock:
    pthread_mutex_unlock(&ctx->lock);
    return retVal;
}

int rm_monitor_remove_process(struct ProcessMonitor *ctx, pid_t pid) {
    log_info("接收到取消监控进程%d的请求", pid);
    pthread_mutex_lock(&ctx->lock);
    for (int i = 0; i < ctx->lenGroups; i++) {
        if (ctx->groups[i]->pids[0] == pid) {
            destroyContext(ctx->groups[i]);
            // 将最后的一个过来
            ctx->groups[i] = ctx->groups[--ctx->lenGroups];
            pthread_mutex_unlock(&ctx->lock);
            log_info("成功取消对进程%d的监控", pid);
            return 0;
        }
    }

    log_warn("监控队列中找不到进程%d", pid);
    pthread_mutex_unlock(&ctx->lock);
    return ESRCH;
}

unsigned int rm_monitor_get_max_process(struct ProcessMonitor *ctx) {
    return ctx->maxRMID;
}

int rm_monitor_add_process_group(struct ProcessMonitor *ctx, pid_t *pidList, int lenPidList, const char *outFile,
                                 struct ProcessMonitorContext **monitorCtx) {
    // 检查进程是否都存在
    for (int i = 0; i < lenPidList; i++) {
        if (processNotExist(pidList[i])) {
            log_error("进程%d不存在", pidList[i]);
            return ESRCH;
        }
    }

    int retVal = 0;
    pthread_mutex_lock(&ctx->lock);
    if (ctx->lenGroups >= ctx->maxRMID) {
        retVal = ERR_MONITOR_FULL;
        goto unlock;
    }

    FILE *file = fopen(outFile, "w");
    if (file == NULL) {
        int err = errno;
        log_error("无法创建文件，原因为%s", strerror(err));
        retVal = err;
        goto unlock;
    }

    char *pidListString = pidListToCommaSeparatedString(pidList, lenPidList);
    log_info("正在将一组进程加入监控，进程为%s", pidListString);
    free(pidListString);
    struct ProcessMonitorContext *mctx = malloc(sizeof(struct ProcessMonitorContext));
    memset(mctx, 0, sizeof(struct ProcessMonitorContext));
    mctx->outFile = file;
    retVal = pqos_mon_start_pids(lenPidList, pidList,
                                 PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                                 PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS, NULL, &mctx->group);
    if (retVal != PQOS_RETVAL_OK) {
        log_info("请求监控进程组失败，返回码为%d", retVal);
        goto unlock;
    }

    ctx->groups = realloc(ctx->groups, (ctx->lenGroups + 1) * sizeof(struct ProcessMonitorContext *));
    ctx->groups[ctx->lenGroups++] = &mctx->group;
    *monitorCtx = mctx;

    unlock:
    pthread_mutex_unlock(&ctx->lock);
    return retVal;
}

int rm_monitor_remove_process_group(struct ProcessMonitor *ctx, struct ProcessMonitorContext *monitorCtx) {
    char *pidListString = pidListToCommaSeparatedString(monitorCtx->group.pids, (int) monitorCtx->group.num_pids);
    log_info("接收到取消监控进程组的请求，进程组进程为：%s", pidListString);
    free(pidListString);

    pthread_mutex_lock(&ctx->lock);
    for (int i = 0; i < ctx->lenGroups; i++) {
        if (container_of(ctx->groups[i], struct ProcessMonitorContext, group) == monitorCtx) {
            destroyContext(ctx->groups[i]);
            // 将最后的一个过来
            ctx->groups[i] = ctx->groups[--ctx->lenGroups];
            pthread_mutex_unlock(&ctx->lock);
            log_info("成功取消对进程组的监控");
            return 0;
        }
    }

    log_warn("监控队列中找不到进程组");
    pthread_mutex_unlock(&ctx->lock);
    return ESRCH;
}
