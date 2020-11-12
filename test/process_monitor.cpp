//
// Created by wjx on 2020/11/11.
//

#include <gtest/gtest.h>

extern "C" {
#include "pqos.h"
#include "../process_monitor.h"
}

#include <cstdlib>

TEST(process_monitor, normal) {
    struct pqos_config config = {
            .fd_log =  STDOUT_FILENO,
            .callback_log = nullptr,
            .context_log = nullptr,
            .verbose = 0,
            .interface = PQOS_INTER_OS,
    };
    pqos_init(&config);
    ProcessMonitor *monitor = monitorCreate(1000);
    ASSERT_EQ(0, monitorAddProcess(monitor, 2099));
    sleep(10);
    monitorRemoveProcess(monitor, 1);
    monitorDestroy(monitor);
}