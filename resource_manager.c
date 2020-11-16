//
// Created by wjx on 2020/11/12.
//

#include "resource_manager.h"

#include <pqos.h>

struct pqos_config config = {
        .fd_log = STDOUT_FILENO,
        .interface = PQOS_INTER_OS,
};

int init;

int rm_init() {
    int retVal = 0;
    if (init == 0) {
        retVal = pqos_init(&config);
        if (retVal == PQOS_RETVAL_OK) {
            init++;
        }
    }

    return retVal;
}

int rm_finalize() {
    int retVal = 0;
    if (init == 1) {
        retVal = pqos_fini();
        if (retVal == PQOS_RETVAL_OK) {
            init--;
        }
    }

    return retVal;
}
