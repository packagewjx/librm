//
// Created by wjx on 2020/12/6.
//

#ifndef RESOURCEMANAGER_EXAMPLE_PROGRAMS_H
#define RESOURCEMANAGER_EXAMPLE_PROGRAMS_H

#include <stdlib.h>

pid_t forkAndRun(void (*program)());

void nopLoop();

void sequenceMemoryAccessor();

void randomMemoryAccessor();

#endif //RESOURCEMANAGER_EXAMPLE_PROGRAMS_H
