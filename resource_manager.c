//
// Created by wjx on 2020/11/12.
//

#include "resource_manager.h"

#include <pqos.h>

struct pqos_config config = {
        .fd_log = STDOUT_FILENO,
        .interface = PQOS_INTER_OS,
};

int init = 0;

int rm_init() {
    if (init == 0) {
        int retVal = pqos_init(&config);
        if (retVal != PQOS_RETVAL_OK) {
            return retVal;
        }
        // 避免之前的错误运行导致资源的浪费
        retVal = pqos_mon_reset();
        if (retVal != PQOS_RETVAL_OK) {
            pqos_fini();
            return retVal;
        }
        init = 1;
    }

    return 0;
}

int rm_finalize() {
    int retVal = 0;
    if (init == 1) {
        retVal = pqos_fini();
        if (retVal == PQOS_RETVAL_OK) {
            init = 0;
        }
    }

    return retVal;
}
