//
// Created by wjx on 2020/11/26.
//

#include "rth.h"
#include "log.h"
#include "utils/general.h"

#define RAND_1 ((float) random() / (float) RAND_MAX)

enum Status {
    TAGGED,
    UNTAGGED,
};

struct rm_mem_sample_entry {
    u_int64_t addr;
    enum Status status;
    u_int64_t firstTime;
    u_int64_t lastTime;
};

int rm_mem_rth_start(int size, struct rm_mem_rth_context *ctx) {
    size = highestBit(size) << 1;
    if (0 != hashmap_create(size, &ctx->reservoir)) {
        return 1;
    }
    ctx->capacity = size;
    ctx->logicalTime = 0;
    return 0;
}

struct findKeyAtContext {
    struct hashmap_element_s *elm;
    int curr;
    int target;
};

int findElementAt(void *const context, struct hashmap_element_s *const e) {
    struct findKeyAtContext *ctx = context;
    if (ctx->curr == ctx->target) {
        ctx->elm = e;
        return 1;
    }
    ctx->curr++;
    // 可能是bug，返回0才会继续遍历
    return 0;
}

int rm_mem_rth_update(struct rm_mem_rth_context *ctx, struct rm_mem_mon_trace_data *traceData, int lenTraceData) {
    int numNew = 0, numTagged = 0, numUntagged = 0, numRemoved = 0;

    for (int i = 0; i < lenTraceData; i++) {
        struct rm_mem_sample_entry *ent = hashmap_get(&ctx->reservoir, (const char *) &traceData[i].addr,
                                                      sizeof(u_int64_t));
        u_int64_t time = ctx->logicalTime++;
        if (ent == NULL) {
            numNew++;
            // 这是全新的地址。集合未满的情况下必定插入，集合已满的情况下按概率插入。
            if (ctx->capacity <= hashmap_num_entries(&ctx->reservoir)) {
                // 在这里，集合已满
                // 判断是否需要剔除
                if (RAND_1 > (float) ctx->capacity / time) {
                    continue;
                }
                // 随机剔除一个。为了提高随机性和节省空间，采用循环查询一个随机位置是否有数据的方法
                int target = (int) ((float) ctx->reservoir.table_size * RAND_1);
                while (!ctx->reservoir.data[target].in_use) {
                    target = (int) ((float) ctx->reservoir.table_size * RAND_1);
                }
                struct hashmap_element_s *toRemove = &ctx->reservoir.data[target];
                void *toFree = toRemove->data;
                log_trace("丢弃地址为%#018lx的记录", *(u_int64_t *) toRemove->key);
                numRemoved++;
                hashmap_remove(&ctx->reservoir, toRemove->key, sizeof(u_int64_t));
                free(toFree);
            }

            // 将新的地址插入到哈希表
            ent = malloc(sizeof(struct rm_mem_sample_entry));
            ent->addr = traceData[i].addr;
            ent->firstTime = time;
            ent->lastTime = time;
            ent->status = UNTAGGED;
            hashmap_put(&ctx->reservoir, (const char *) &ent->addr, sizeof(u_int64_t), ent);
        } else if (ent->status == UNTAGGED) {
            numUntagged++;
            ent->status = TAGGED;
            ent->lastTime = time;
        } else {
            numTagged++;
        }
        // TAGGED的不做任何处理
    }
    log_info("RTH更新状况：新插入%d，Tagged %d，Untagged %d，删除记录%d个", numNew, numTagged, numUntagged, numRemoved);
    return 0;
}

int calculateReuseTimeAndFree(void *const context, void *const elm) {
    struct rm_mem_rth *result = context;
    struct rm_mem_sample_entry *ent = elm;
    u_int64_t reuseTime = ent->lastTime - ent->firstTime;
    if (reuseTime > result->maxTime) {
        result->occurrence[result->maxTime + 1]++;
    } else {
        result->occurrence[reuseTime]++;
    }

    free(ent);
    return 1;
}

int rm_mem_rth_finish(struct rm_mem_rth_context *ctx, struct rm_mem_rth **rth, int maxTime) {
    log_info("结束采样并计算RTH中。Reservoir大小：%d", hashmap_num_entries(&ctx->reservoir));
    int *occurrence = malloc((maxTime + 2) * sizeof(int));
    memset(occurrence, 0, (maxTime + 2) * sizeof(int));
    struct rm_mem_rth *result = malloc(sizeof(struct rm_mem_rth));
    result->maxTime = maxTime;
    result->occurrence = occurrence;
    hashmap_iterate(&ctx->reservoir, calculateReuseTimeAndFree, result);
    hashmap_destroy(&ctx->reservoir);
    *rth = result;
    if (occurrence[maxTime + 1] > 0) {
        log_warn("RTH中存在%d条重使用时间大于%d的记录", occurrence[maxTime + 1], maxTime);
    }
    return 0;
}

int rm_mem_rth_destroy(struct rm_mem_rth *rth) {
    free(rth->occurrence);
    free(rth);
    return 0;
}
