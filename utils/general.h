//
// Created by wjx on 2020/11/11.
//

#ifndef RESOURCEMANAGER_GENERAL_H
#define RESOURCEMANAGER_GENERAL_H

#include "stdlib.h"

unsigned long getCurrentTimeMilli();

char *pidListToCommaSeparatedString(pid_t *pidList, int pidListLen);

char *joinString(char **str, int lenStr, char sep);

#endif //RESOURCEMANAGER_GENERAL_H
