//
// Created by wjx on 2020/11/11.
//

#ifndef RESOURCEMANAGER_GENERAL_H
#define RESOURCEMANAGER_GENERAL_H

#include "stdlib.h"

unsigned long getCurrentTimeMilli();

char *pidListToCommaSeparatedString(pid_t *pidList, int pidListLen);

char *joinString(char **str, int lenStr, char sep);

int highestBit(int n);

/**
 * 查看进程是否存在且正在运行
 * @param pid
 * @retval 0 正在运行
 * @retval -1 进程不存在
 * @retval -2 进程不在运行
 * @retval 其他 查询失败
 */
int processRunning(pid_t pid);

/**
 * 递归删除文件
 * @param path 若为文件名，则直接删除。若为文件夹，则进入删除
 * @return 执行结果。错误值参考remove的错误
 * @retval 0 执行成功
 */
int recursivelyRemove(const char * path);

#endif //RESOURCEMANAGER_GENERAL_H
