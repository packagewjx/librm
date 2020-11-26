//
// Created by wjx on 2020/11/26.
//

#ifndef RESOURCEMANAGER_RTH_H
#define RESOURCEMANAGER_RTH_H

#include "perfmem.h"
#include "hashmap.h"

struct rm_mem_rth_context {
    u_int64_t logicalTime;
    int capacity;
    struct hashmap_s reservoir;
};


struct rm_mem_rth {
    int *occurrence;
    int maxTime;
};

/**
 * 初始化rm_mem_sample_data，准备接收采样的数据。
 * 本采样使用Reservoir Sampling方法。
 * @param size [in] 监控集合容量
 * @param ctx [out] 数据指针
 * @return
 */
int rm_mem_rth_start(int size, struct rm_mem_rth_context *ctx);

/**
 * 使用当前采样数据更新数据集
 * @param ctx [in]
 * @param traceData [in] 按顺序采样得到的结果。每次读取一条数据都将会给data的逻辑时间加1。
 * @param lenTraceData [in]
 * @return
 */
int rm_mem_rth_update(struct rm_mem_rth_context *ctx, struct rm_mem_mon_trace_data *traceData, int lenTraceData);

/**
 * 结束采样并计算RTH
 * @param ctx [in] 结束采样，并将数据计算出对应的RTH
 * @param rth [out] 计算得出的RTH
 * @return 执行结果
 * @retval 0 执行成功
 */
int rm_mem_rth_finish(struct rm_mem_rth_context *ctx, struct rm_mem_rth **rth, int maxTime);

/**
 * rth使用完毕后回收资源
 * @param rth
 * @return 执行结果
 */
int rm_mem_rth_destroy(struct rm_mem_rth *rth);

#endif //RESOURCEMANAGER_RTH_H
