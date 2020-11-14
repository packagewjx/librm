#include "pqos_utils.h"
#include "pqos.h"
#include "stdlib.h"
#include "string.h"

unsigned int *GetAllCoresId(const struct pqos_cpuinfo *cpu, unsigned int *count) {
    *count = 0;
    int coreCount = 0;
    unsigned int *result = malloc(sizeof(unsigned int));
    int socket = 0;
    unsigned int *socketResult = pqos_cpu_get_cores(cpu, socket, &coreCount);
    while (socketResult != NULL) {
        int lastAll = *count;
        *count += coreCount;
        result = realloc(result, *count * sizeof(unsigned int));
        memcpy(result + lastAll, socketResult, coreCount * sizeof(unsigned int));
        // Query Next Socket
        free(socketResult);
        socket++;
        socketResult = pqos_cpu_get_cores(cpu, socket, &coreCount);
    }

    return result;
}

unsigned int *GetAllSocketId(int *numSockets) {
    struct pqos_cpuinfo *cpu;

}
