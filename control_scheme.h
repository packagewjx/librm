//
// Created by wjx on 2020/11/12.
//

#ifndef RESOURCEMANAGER_CONTROL_SCHEME_H
#define RESOURCEMANAGER_CONTROL_SCHEME_H

#include <stdlib.h>

#define ERR_TOO_MANY_CAT 101
#define ERR_TOO_MANY_MBA 100
#define ERR_EMPTY_PID_LIST 102
#define ERR_NULL_PTR 2
#define ERR_UNKNOWN 1

struct CLOSScheme {
    int closNum; // CLOS号码
    pid_t *processList;
    unsigned int lenProcessList;
    unsigned int llc;
    unsigned int mbaThrottle;
};

struct CLOSCapabilityInfo {
    unsigned int numCatClos; // CAT最大方案数量
    unsigned int maxLLCWays; // 缓存路数量
    unsigned int minLLCWays; // 最少需要设置的缓存路掩码
    unsigned int numMbaClos; // MBA最大方案数量
};

/**
 * 设置当前的内存与带宽控制。注意，不能设置CLOS 0。
 * 本函数非线程安全
 *
 * @param schemes 控制方案数组。其中，如果llc或这mbaThrottle为0的话，则会不设置该值。可以用于单纯修改pid。
 * @param lenSchemes 数组长度
 * @return 执行结果
 * @retval 0 修改控制方案成功
 * @retval ERR_TOO_MANY_CAT CAT方案过多
 * @retval ERR_TOO_MANY_MBA MBA方案过多
 * @retval 其他 修改失败
 */
int controlSchemeSet(struct CLOSScheme *schemes, int lenSchemes);

/**
 * 获取用于分配的有用信息
 * @param info [out] 存放结果的目录
 * @return 操作结果
 * @retval 0 成功
 */
int controlSchemeGetInfo(struct CLOSCapabilityInfo *info);

#endif //RESOURCEMANAGER_CONTROL_SCHEME_H
