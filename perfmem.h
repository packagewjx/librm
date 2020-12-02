//
// Created by wjx on 2020/11/25.
//

#ifndef RESOURCEMANAGER_PERFMEM_H
#define RESOURCEMANAGER_PERFMEM_H

#include <stdlib.h>

struct rm_mem_mon_data {
    const char* groupId;
    pid_t perfMemPid;
    pid_t perfStatPid;
    const char *perfDataDir;
};

struct rm_mem_mon_trace_data {
    u_int64_t addr;
};

/**
 * 启动perf进程监控pidList的内存读写
 * @param pidList [in]
 * @param lenPid [in]
 * @param data [out] 存储数据的地方
 * @return 执行情况
 * @retval 0 执行成功
 */
int rm_mem_mon_start(pid_t *pidList, int lenPid, struct rm_mem_mon_data *data, const char *requestId);

/**
 * 获取数据
 * @param data 监控组数据
 * @param recordLen [out] 数据数组的长度
 * @param records [out] 数据数组指针的存放位置。使用完毕后应该free掉。
 * @return 执行结果
 * @retval 0 执行成功
 */
int rm_mem_mon_poll(struct rm_mem_mon_data *data, int *recordLen, struct rm_mem_mon_trace_data **records);

int rm_mem_mon_stop(struct rm_mem_mon_data *data);

#endif //RESOURCEMANAGER_PERFMEM_H
