//
// Created by wjx on 2020/11/12.
//

#ifndef RESOURCEMANAGER_RESOURCEMANAGER_H
#define RESOURCEMANAGER_RESOURCEMANAGER_H

#include <stdlib.h>

/**
 * ERRORS Definitions
 */

#define ERR_UNKNOWN 1
#define ERR_NULL_PTR 2
#define ERR_TOO_MANY_CAT 100
#define ERR_TOO_MANY_MBA 101
#define ERR_EMPTY_PID_LIST 102
#define ERR_MONITOR_FULL 103
#define ERR_DUPLICATE_PID 104

/**
 * ================================================
 * Library Init and Finalize
 * ================================================
 */

/**
 * 初始化库的函数。
 * 非线程安全。
 * @return 操作结果
 * @retval 0 操作成功
 */
int rm_init();

/**
 * 使用库结束后关闭的函数。
 * 非线程安全。
 * @return 操作结果
 * @retval 0 关闭成功
 */
int rm_finalize();

/**
 * ================================================
 * Process Monitor
 * ================================================
 */

struct ProcessMonitor;

struct ProcessMonitorContext;

/**
 * 创建Monitor实例
 * @param sleepMilli 取样间隔时长，单位为毫秒
 * @return 实例指针
 * @retval NULL 代表创建失败
 * @retval 非NULL 代表创建成功
 */
struct ProcessMonitor *rm_monitor_create(unsigned int sleepMilli);

/**
 * 关闭Monitor实例并回收资源
 * @return 执行结果
 * @retval 0 代表执行成功
 * @retval 非0 代表执行失败
 */
int rm_monitor_destroy(struct ProcessMonitor *ctx);

/**
 * 添加pid进程到当前监控的进程
 * 注意，若进程在监控途中结束，不会产生任何错误，但是会导致监控资源的浪费
 *
 * @return 执行结果
 * @retval 0 成功
 * @retval ERR_MONITOR_FULL 代表不能再添加监控
 * @retval ESRCH 代表进程不存在
 * @retval 其他值 失败
 */
int rm_monitor_add_process(struct ProcessMonitor *ctx, pid_t pid);

/**
 * 从当前监控的进程中删除pid进程
 *
 * @return 执行结果
 * @retval 0 代表删除成功，值得注意的是若pid并不存在于监控范围也算是成功
 * @retval ESRCH 代表找不到这个进程
 * @retval 非0 代表失败
 */
int rm_monitor_remove_process(struct ProcessMonitor *ctx, pid_t pid);

/**
 * 添加一组进程监控，监控的结果将会写入到指定的文件中
 * @param monitorCtx [out]
 * @return 执行结果
 * @retval 0 执行成功
 * @retval ESRCH 进程列表中的进程不存在
 * @retval ERR_MONITOR_FULL 无法再添加进程
 */
int rm_monitor_add_process_group(struct ProcessMonitor *ctx, pid_t *pidList, int lenPidList, const char *outFile,
                                 struct ProcessMonitorContext **monitorCtx);

/**
 * 停止监控进程组
 * @param monitorCtx [in] 受监控的进程组信息
 * @return 执行结果
 * @retval 0 执行成功
 * @retval
 */
int rm_monitor_remove_process_group(struct ProcessMonitor *ctx, struct ProcessMonitorContext *monitorCtx);


/**
 * 获取最大能够同时监控的进程数量
 * @return 最大数量
 */
unsigned int rm_monitor_get_max_process(struct ProcessMonitor *ctx);

/**
 * ================================================
 * Control Scheme
 * ================================================
 */

struct rm_clos_scheme {
    int closNum; // CLOS号码
    pid_t *processList;
    unsigned int lenProcessList;
    unsigned int llc;
    unsigned int mbaThrottle;
};

struct rm_capability_info {
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
int rm_control_scheme_set(struct rm_clos_scheme *schemes, int lenSchemes);

/**
 * 获取用于分配的有用信息
 * @param info [out] 存放结果的目录
 * @return 操作结果
 * @retval 0 成功
 */
int rm_get_capability_info(struct rm_capability_info *info);

#endif //RESOURCEMANAGER_RESOURCEMANAGER_H
