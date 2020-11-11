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
    testPoll(cores, coreCount, pid);

    pqos_fini();

    return 0;
}

void testPoll(unsigned int *cores, unsigned int numCore, pid_t pid) {
    struct pqos_mon_data group;
    pqos_mon_start(numCore, cores, PQOS_MON_EVENT_LMEM_BW | PQOS_MON_EVENT_RMEM_BW | PQOS_MON_EVENT_L3_OCCUP, NULL,
                   &group);
    pqos_mon_add_pids(1, &pid, &group);
    sleep(1);
    struct pqos_mon_data *groups[] = {&group};

    for (int i = 0; i < 10000; i++) {
        pqos_mon_poll(groups, 1);
        printf("%ld %ld\n", group.values.mbm_local, group.values.mbm_local_delta);
    }
    pqos_mon_stop(&group);
}