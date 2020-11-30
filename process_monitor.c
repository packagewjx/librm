//
// Created by wjx on 2020/11/10.
//

#include "resource_manager.h"
#include "utils/general.h"
#include "log.h"
#include "rth.h"
#include "perfmem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pqos.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

struct ProcessMonitorContext {
    const char *groupId; // 组Id
    FILE *outFile; // 输出csv记录的文件
    struct pqos_mon_data monData;
    struct rm_mem_mon_data memMon;
    struct rm_mem_rth_context rthCtx;
};

struct ProcessMonitorRecord {
    const char *groupId;
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
    struct ProcessMonitorContext **ctxList;
    unsigned int lenCtxList;
    unsigned int maxRMID;
    unsigned int reservoirSize;
    unsigned int maxRthTime;
};

/**
 * 将Pqos record写入到file中
 * @return 成功的话返回大于0的数，失败返回小于0的数。
 */
extern inline int writePqosRecord(struct ProcessMonitorRecord *record, FILE *file) {
    return fprintf(file, "%s,%ld,%ld,%ld,%ld,%ld,%f\n", record->groupId, record->timestamp, record->localMemoryDelta,
                   record->remoteMemoryDelta, record->llc, record->llcMisses, record->ipc);
}

extern inline int pollPqosMon(struct ProcessMonitorContext *ctx) {
    struct pqos_mon_data *groupList[] = {&ctx->monData};
    int retVal = pqos_mon_poll(groupList, 1);
    if (retVal != PQOS_RETVAL_OK) {
        return retVal;
    }

    struct ProcessMonitorRecord record;
    record.groupId = ctx->groupId;
    record.llc = ctx->monData.values.llc;
    record.timestamp = getCurrentTimeMilli();
    record.llcMisses = ctx->monData.values.llc_misses;
    record.ipc = ctx->monData.values.ipc;
    record.localMemoryDelta = ctx->monData.values.mbm_local_delta;
    record.remoteMemoryDelta = ctx->monData.values.mbm_remote_delta;
    writePqosRecord(&record, ctx->outFile);
    return 0;
}

extern inline int pollPerfMon(struct ProcessMonitorContext *ctx) {
    int recordLen;
    struct rm_mem_mon_trace_data *records = NULL;
    int retVal = rm_mem_mon_poll(&ctx->memMon, &recordLen, &records);
    if (retVal != 0) {
        goto ret;
    }
    retVal = rm_mem_rth_update(&ctx->rthCtx, records, recordLen);

    free(records);
    ret:
    return retVal;
}

/**
 * 回收mon_data资源
 * @param ctx 必须是ProcessMonitorContext中的group成员，否则结果是不可知的
 */
extern inline void finishMonitor(struct ProcessMonitorContext *ctx, unsigned int maxRthTime) {
    log_info("正在结束进程组%s的监控", ctx->groupId);
    fclose(ctx->outFile);
    pqos_mon_stop(&ctx->monData);
    rm_mem_mon_stop(&ctx->memMon);
    struct rm_mem_rth *rth;
    if (0 == rm_mem_rth_finish(&ctx->rthCtx, &rth, maxRthTime)) {
        // 写入到文件
        char outFileName[FILENAME_MAX];
        sprintf(outFileName, "%s.rth.csv", ctx->groupId);
        FILE *outFile = fopen(outFileName, "w");
        if (outFile != NULL) {
            for (int i = 0; i <= rth->maxTime; i++) {
                fprintf(outFile, "%d,%d\n", i, rth->occurrence[i]);
            }
            fclose(outFile);
        } else {
            log_error("创建RTH输出文件%s出错，返回码%d", outFileName, errno);
        }
    } else {
        log_error("计算进程组%s的RTH出错", ctx->groupId);
    }

    free((void *) ctx->groupId);
    free(ctx);
    rm_mem_rth_destroy(rth);
}

extern inline int processNotExist(pid_t pid) {
    return kill(pid, 0) == -1;
}

void *monitorThread(void *args) {
    struct ProcessMonitor *ctx = args;
    struct timespec sleepTime = {
            .tv_sec = ctx->sleepMilli / 1000,
            .tv_nsec = ctx->sleepMilli % 1000 * 1000000
    };
    log_info("进程监控线程启动");

    while (ctx->running) {
        pthread_mutex_lock(&ctx->lock);
        if (ctx->ctxList == 0) {
            log_info("目前没有进程监控，等待下一次唤醒");
            goto unlockAndSleep;
        }

        log_info("正在抓取%d个监控进程的监控数据", ctx->lenCtxList);
        for (int i = 0; i < ctx->lenCtxList; i++) {
            int retVal = pollPqosMon(ctx->ctxList[i]);
            if (retVal != 0) {
                log_error("获取进程组%s的pqos数据出错", ctx->ctxList[i]->groupId);
            }
            retVal = pollPerfMon(ctx->ctxList[i]);
            if (retVal != 0) {
                log_error("获取进程组%s的perf数据出错", ctx->ctxList[i]->groupId);
            }
        }

        unlockAndSleep:
        pthread_mutex_unlock(&ctx->lock);
        nanosleep(&sleepTime, NULL);
    }

    log_info("进程监控线程退出");
    return NULL;
}

struct ProcessMonitor *rm_monitor_create(unsigned int sleepMilli, unsigned int reservoirSize, unsigned int maxRthTime) {
    const struct pqos_cap *cap;
    const struct pqos_cpuinfo *cpu;
    if (PQOS_RETVAL_OK != pqos_cap_get(&cap, &cpu)) {
        return NULL;
    }

    struct ProcessMonitor *ctx = malloc(sizeof(struct ProcessMonitor));
    ctx->sleepMilli = sleepMilli;
    ctx->running = true;
    pthread_mutex_init(&ctx->lock, NULL);
    ctx->ctxList = malloc(0);
    ctx->lenCtxList = 0;
    ctx->maxRMID = cap->capabilities->u.mon->max_rmid;
    ctx->reservoirSize = reservoirSize;
    ctx->maxRthTime = maxRthTime;
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
    for (int i = 0; i < ctx->lenCtxList; i++) {
        finishMonitor(ctx->ctxList[i], ctx->maxRthTime);
    }

    free(ctx->ctxList);
    ctx->lenCtxList = 0;

    pthread_mutex_destroy(&ctx->lock);

    free(ctx);

    log_info("监控成功关闭");
    return 0;
}

unsigned int rm_monitor_get_max_process(struct ProcessMonitor *ctx) {
    return ctx->maxRMID;
}

int rm_monitor_add_process_group(struct ProcessMonitor *ctx, pid_t *pidList, int lenPidList, const char *groupId,
                                 struct ProcessMonitorContext **monitorCtx) {
    // 检查进程是否都存在
    for (int i = 0; i < lenPidList; i++) {
        if (processNotExist(pidList[i])) {
            log_error("进程%d不存在", pidList[i]);
            return ESRCH;
        }
    }

    int retVal;
    pthread_mutex_lock(&ctx->lock);

    // 检查是否超过监控限制
    if (ctx->lenCtxList >= ctx->maxRMID) {
        retVal = ERR_MONITOR_FULL;
        goto unSuccess;
    }

    // 检查是否有重复的
    for (int i = 0; i < ctx->lenCtxList; i++) {
        if (strcmp(ctx->ctxList[i]->groupId, groupId) == 0) {
            retVal = ERR_DUPLICATE_GROUP;
            goto unSuccess;
        }
    }

    char pqosFileName[FILENAME_MAX];
    sprintf(pqosFileName, "%s.pqos.csv", groupId);
    FILE *pqosFile = fopen(pqosFileName, "w");
    if (pqosFile == NULL) {
        int err = errno;
        log_error("无法创建文件%s，原因为%s", pqosFileName, strerror(err));
        retVal = err;
        goto unSuccess;
    }
    fcntl(fileno(pqosFile), F_SETFD, FD_CLOEXEC);

    char *pidListString = pidListToCommaSeparatedString(pidList, lenPidList);
    log_info("正在将进程组%s加入监控，进程为%s", groupId, pidListString);
    free(pidListString);

    struct ProcessMonitorContext *mctx = malloc(sizeof(struct ProcessMonitorContext));
    memset(mctx, 0, sizeof(struct ProcessMonitorContext));
    // 开始pqos监控
    retVal = pqos_mon_start_pids(lenPidList, pidList,
                                 PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                                 PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS, NULL, &mctx->monData);
    if (retVal != PQOS_RETVAL_OK) {
        fclose(pqosFile);
        log_error("请求pqos监控进程组%s失败，返回码为%d", groupId, retVal);
        goto unSuccess;
    }
    // 开始perf监控
    retVal = rm_mem_mon_start(pidList, lenPidList, &mctx->memMon);
    if (retVal != 0) {
        fclose(pqosFile);
        pqos_mon_stop(&mctx->monData);
        log_error("请求perf监控进程组%s失败，返回码为%d", groupId, retVal);
        goto unSuccess;
    }
    // 初始化RTH
    retVal = rm_mem_rth_start(ctx->reservoirSize, &mctx->rthCtx);
    if (retVal != 0) {
        fclose(pqosFile);
        pqos_mon_stop(&mctx->monData);
        rm_mem_mon_stop(&mctx->memMon);
        log_error("创建rth失败，返回码为%d", groupId, retVal);
        goto unSuccess;
    }
    // 赋值剩余内容
    mctx->groupId = malloc(strlen(groupId) + 1);
    strcpy((char *) mctx->groupId, groupId);
    mctx->outFile = pqosFile;

    // 加入到列表，完成整个初始化过程
    ctx->ctxList = realloc(ctx->ctxList, (ctx->lenCtxList + 1) * sizeof(struct ProcessMonitorContext *));
    ctx->ctxList[ctx->lenCtxList++] = mctx;
    *monitorCtx = mctx;
    pthread_mutex_unlock(&ctx->lock);
    log_info("成功将进程组%s加入监控", groupId);
    return 0;

    unSuccess:
    pthread_mutex_unlock(&ctx->lock);
    return retVal;
}

int rm_monitor_remove_process_group(struct ProcessMonitor *ctx, struct ProcessMonitorContext *monitorCtx) {
    log_info("接收到取消监控进程组%s的请求", monitorCtx->groupId);

    pthread_mutex_lock(&ctx->lock);
    for (int i = 0; i < ctx->lenCtxList; i++) {
        if (ctx->ctxList[i] == monitorCtx) {
            finishMonitor(ctx->ctxList[i], ctx->maxRthTime);
            // 将最后的一个过来
            ctx->ctxList[i] = ctx->ctxList[--ctx->lenCtxList];
            pthread_mutex_unlock(&ctx->lock);
            log_info("已取消对进程组的监控");
            return 0;
        }
    }
    pthread_mutex_unlock(&ctx->lock);

    log_warn("监控队列中找不到进程组%s", monitorCtx->groupId);
    return ESRCH;
}
