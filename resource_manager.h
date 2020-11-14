//
// Created by wjx on 2020/11/12.
//

#ifndef RESOURCEMANAGER_RESOURCEMANAGER_H

#include "control_scheme.h"
#include "process_monitor.h"

/**
 * 初始化库的函数。
 * 非线程安全。
 * @return 操作结果
 * @retval 0 操作成功
 */
int resourceManagerInit();

/**
 * 使用库结束后关闭的函数。
 * 非线程安全。
 * @return 操作结果
 * @retval 0 关闭成功
 */
int resourceManagerFinalize();

#define RESOURCEMANAGER_RESOURCEMANAGER_H

#endif //RESOURCEMANAGER_RESOURCEMANAGER_H
