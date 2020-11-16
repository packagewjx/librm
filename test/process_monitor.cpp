//
// Created by wjx on 2020/11/11.
//

#include <gtest/gtest.h>

extern "C" {
#include <fcntl.h>
#include "pqos.h"
#include "../resource_manager.h"
}

#include <cstdlib>

void checkCsv(const char *fileName);

class ProcessMonitorTest : public ::testing::Test {
protected:
    struct pqos_config config{};
    struct pqos_cap *cap = nullptr;
    struct pqos_cpuinfo *cpu = nullptr;

    ProcessMonitorTest() {
        memset(&this->config, 0, sizeof(struct pqos_config));
        this->config.fd_log = STDOUT_FILENO;
        this->config.interface = PQOS_INTER_OS;
    }

    void SetUp() override {
        pqos_init(&this->config);
        pqos_cap_get((const pqos_cap **) &cap, (const pqos_cpuinfo **) &cpu);
    }

    void TearDown() override {
        pqos_fini();
    }
};

TEST_F(ProcessMonitorTest, single_process) {
    ProcessMonitor *monitor = rm_monitor_create(100);
    ASSERT_NE(nullptr, monitor);
    ASSERT_EQ(0, rm_monitor_add_process(monitor, 1));
    sleep(1);
    rm_monitor_remove_process(monitor, 1);
    rm_monitor_destroy(monitor);

    // 检查文件存在
    checkCsv("1.csv");
}

TEST_F(ProcessMonitorTest, multiple_process) {
    ProcessMonitor *monitor = rm_monitor_create(100);
    ASSERT_NE(nullptr, monitor);
    ASSERT_EQ(0, rm_monitor_add_process(monitor, 1));
    ASSERT_EQ(0, rm_monitor_add_process(monitor, 2));
    sleep(1);
    rm_monitor_remove_process(monitor, 1);
    rm_monitor_remove_process(monitor, 2);
    rm_monitor_destroy(monitor);

    checkCsv("1.csv");
    checkCsv("2.csv");
}

TEST_F(ProcessMonitorTest, illeagl_process) {
    ProcessMonitor *monitor = rm_monitor_create(1000000);
    ASSERT_NE(nullptr, monitor);

    // 不存在的
    ASSERT_EQ(ESRCH, rm_monitor_add_process(monitor, 1000000));

    // 重复的
    ASSERT_EQ(0, rm_monitor_add_process(monitor, 1));
    ASSERT_EQ(ERR_DUPLICATE_PID, rm_monitor_add_process(monitor, 1));
}

TEST_F(ProcessMonitorTest, remove_process) {
    ProcessMonitor *m = rm_monitor_create(10000);
    ASSERT_NE(nullptr, m);
    ASSERT_EQ(0, rm_monitor_add_process(m, 1));
    ASSERT_EQ(0, rm_monitor_add_process(m, 2));
    ASSERT_EQ(0, rm_monitor_add_process(m, getpid()));

    ASSERT_EQ(0, rm_monitor_remove_process(m, getpid()));
    ASSERT_EQ(0, rm_monitor_remove_process(m, 1));
    ASSERT_EQ(0, rm_monitor_remove_process(m, 2));
}

TEST(process_monitor, init_fail) {
    ProcessMonitor *p = rm_monitor_create(100);
    ASSERT_EQ(nullptr, p);
}

void checkCsv(const char* fileName) {
    int fd = open(fileName, O_RDONLY);
    ASSERT_NE(-1, fd);

    struct stat fdstat{};
    fstat(fd, &fdstat);
    ASSERT_LT(0, fdstat.st_size);

    close(fd);
}
