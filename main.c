#include <pqos.h>
#include <stdlib.h>
#include <string.h>
#include "utils/pqos_utils.h"

void testPoll(unsigned int *cores, unsigned int numCore, pid_t pid);

int main(int argc, char const *argv[]) {
    struct pqos_config config = {
            .interface = PQOS_INTER_OS,
            .fd_log = STDOUT_FILENO,
    };
    pqos_init(&config);
    const struct pqos_cap *cap = NULL;
    const struct pqos_cpuinfo *cpu = NULL;
    pqos_cap_get(&cap, &cpu);
    unsigned int coreCount = 0;
    unsigned int *cores = GetAllCoresId(cpu, &coreCount);
    printf("%d\n", coreCount);

    pid_t pid = strtol(argv[1], NULL, 10);

    struct pqos_mba mba = {
            .class_id = 1,
            .ctrl = PQOS_MBA_ANY,
            .mb_max = 100,
    };
    int ret = pqos_mba_set(0, 1, &mba, NULL);
    printf("%d\n", ret);



    pqos_fini();

    return 0;
}

