//
// Created by wjx on 2020/11/10.
//

#ifndef RESOURCEMANAGER_PROCESS_MONITOR_H
#define RESOURCEMANAGER_PROCESS_MONITOR_H

#include <pthread.h>
#include <stdbool.h>

struct ProcessMonitor;

/**
 * 创建Monitor实例
 * @param sleepMilli 取样间隔时长，单位为毫秒
 * @return 实例指针
 * @retval NULL 代表创建失败
 * @retval 非NULL 代表创建成功
 */
struct ProcessMonitor *monitorCreate(unsigned int sleepMilli);

/**
 * 关闭Monitor实例并回收资源
 * @return 执行结果
 * @retval 0 代表执行成功
 * @retval 非0 代表执行失败
 */
int monitorDestroy(struct ProcessMonitor *ctx);

/**
 * 添加pid进程到当前监控的进程
 *
 * @return 执行结果
 * @retval 0 成功
 * @retval 其他值 失败
 */
int monitorAddProcess(struct ProcessMonitor *ctx, pid_t pid);

/**
 * 从当前监控的进程中删除pid进程
 *
 * @return 执行结果
 * @retval 0 代表删除成功，值得注意的是若pid并不存在于监控范围也算是成功
 * @retval 非0 代表失败
 */
int monitorRemoveProcess(struct ProcessMonitor *ctx, pid_t pid);

#endif //RESOURCEMANAGER_PROCESS_MONITOR_H
