#ifndef PQOS_UTILS_H
#define PQOS_UTILS_H

#include <pqos.h>

unsigned int *GetAllCoresId(const struct pqos_cpuinfo *cpu, unsigned int *count);

#endif