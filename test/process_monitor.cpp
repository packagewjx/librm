//
// Created by wjx on 2020/11/11.
//

#include <gtest/gtest.h>

extern "C" {
#include <fcntl.h>
#include "pqos.h"
#include "../resource_manager.h"
#include "log.h"
#include "../utils/example_programs.h"
}

#include <cstdlib>

void checkPqosCsv(const char *fileName);

void checkRTHCsv(const char *fileName);

void checkPerfStatCsv(const char *fileName);

class ProcessMonitorTest : public ::testing::Test {
protected:
    struct pqos_config config{};
    struct pqos_cap *cap = nullptr;
    struct pqos_cpuinfo *cpu = nullptr;

    ProcessMonitorTest() {
        log_set_level(LOG_DEBUG);
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
    ProcessMonitor *monitor = rm_monitor_create(300, 0x20000, 100000);
    ASSERT_NE(nullptr, monitor);
    pid_t list = getpid();
    ProcessMonitorContext *ctx;
    ASSERT_EQ(0, rm_monitor_add_process_group(monitor, &list, 1, "test", &ctx));
    sleep(3);
    rm_monitor_remove_process_group(monitor, ctx);
    rm_monitor_destroy(monitor);

    // 检查文件存在
    checkPqosCsv("test.pqos.csv");
    checkRTHCsv("test.rth.csv");
    checkPerfStatCsv("test.api.csv");
}

TEST_F(ProcessMonitorTest, multiple_process_groups) {
    ProcessMonitor *monitor = rm_monitor_create(100, 0x20000, 100000);
    ASSERT_NE(nullptr, monitor);
    pid_t list1 = 1;
    pid_t list2 = 2;
    ProcessMonitorContext *c1, *c2;
    ASSERT_EQ(0, rm_monitor_add_process_group(monitor, &list1, 1, "test-1", &c1));
    ASSERT_EQ(0, rm_monitor_add_process_group(monitor, &list2, 1, "test-2", &c2));
    sleep(1);
    rm_monitor_remove_process_group(monitor, c1);
    rm_monitor_remove_process_group(monitor, c2);
    rm_monitor_destroy(monitor);

    checkPqosCsv("test-1.pqos.csv");
    checkPqosCsv("test-2.pqos.csv");
}

TEST_F(ProcessMonitorTest, illeagl_process) {
    ProcessMonitor *monitor = rm_monitor_create(500, 0x20000, 100000);
    ASSERT_NE(nullptr, monitor);

    pid_t list = 100000000;
    ProcessMonitorContext *ctx;
    // 不存在的
    ASSERT_EQ(ESRCH, rm_monitor_add_process_group(monitor, &list, 1, "not-exist", &ctx));

    // 重复的
    list = 1;
    ASSERT_EQ(0, rm_monitor_add_process_group(monitor, &list, 1, "repeated", &ctx));
    ASSERT_EQ(ERR_DUPLICATE_GROUP, rm_monitor_add_process_group(monitor, &list, 1, "repeated", &ctx));
    rm_monitor_destroy(monitor);
}

TEST_F(ProcessMonitorTest, remove_process) {
    ProcessMonitor *m = rm_monitor_create(1000, 0x20000, 100000);
    ASSERT_NE(nullptr, m);
    pid_t list1 = 1;
    pid_t list2 = 2;
    pid_t list3 = getpid();
    ProcessMonitorContext *c1, *c2, *c3;
    ASSERT_EQ(0, rm_monitor_add_process_group(m, &list1, 1, "test-1", &c1));
    ASSERT_EQ(0, rm_monitor_add_process_group(m, &list2, 1, "test-2", &c2));
    ASSERT_EQ(0, rm_monitor_add_process_group(m, &list3, 1, "test-3", &c3));

    ASSERT_EQ(0, rm_monitor_remove_process_group(m, c1));
    ASSERT_EQ(0, rm_monitor_remove_process_group(m, c2));
    ASSERT_EQ(0, rm_monitor_remove_process_group(m, c3));
    rm_monitor_destroy(m);
}

TEST_F(ProcessMonitorTest, process_stop_during_monitor) {
    pid_t childPid = fork();
    if (childPid == 0) {
        // in child
        sleep(1);
        exit(0);
    } else {
        ProcessMonitor *m = rm_monitor_create(500, 0x20000, 100000);
        timespec waitTime = {
                .tv_sec = 0,
                .tv_nsec = 100000000
        };
        ASSERT_NE(nullptr, m);
        // wait for child
        while (kill(childPid, 0) == -1) {
            nanosleep(&waitTime, nullptr);
        }
        ProcessMonitorContext *c1;
        ASSERT_EQ(0, rm_monitor_add_process_group(m, &childPid, 1, "child", &c1));
        sleep(2);
        rm_monitor_destroy(m);
    }
}

TEST_F(ProcessMonitorTest, process_group_monitor) {
    ProcessMonitor *m = rm_monitor_create(200, 0x20000, 100000);
    pid_t list[2] = {1, getpid()};
    ProcessMonitorContext *ctx;
    int retVal = rm_monitor_add_process_group(m, list, 2, "test.csv", &ctx);
    ASSERT_EQ(0, retVal);
    sleep(1);
    retVal = rm_monitor_remove_process_group(m, ctx);
    ASSERT_EQ(0, retVal);
    checkPqosCsv("test.pqos.csv");
    checkRTHCsv("test.rth.csv");
    checkPerfStatCsv("test.api.csv");
    rm_monitor_destroy(m);
}

TEST_F(ProcessMonitorTest, example_program) {
    log_set_level(LOG_DEBUG);
    pid_t pid = forkAndRun(sequenceMemoryAccessor);
    ProcessMonitor *m = rm_monitor_create(1000, 0x20000, 100000);
    ProcessMonitorContext *context;
    ASSERT_EQ(0, rm_monitor_add_process_group(m, &pid, 1, "sequenceMemoryAccessor", &context));
    int status;
    waitpid(pid, &status, 0);
    rm_monitor_destroy(m);
}

TEST(process_monitor, init_fail) {
    ProcessMonitor *p = rm_monitor_create(100, 0x20000, 100000);
    ASSERT_EQ(nullptr, p);
}


void checkPqosCsv(const char *fileName) {
    int fd = open(fileName, O_RDONLY);
    ASSERT_NE(-1, fd);

    struct stat fdstat{};
    fstat(fd, &fdstat);
    ASSERT_LT(0, fdstat.st_size);

    close(fd);
}

void checkRTHCsv(const char *fileName) {
    FILE *f = fopen(fileName, "r");
    ASSERT_NE(nullptr, f);
    char line[4096];
    int nonZeroCount = 0;
    while (fgets(line, 4096, f) != nullptr) {
        int len = strlen(line);
        if (!(line[len - 1] == '0' && line[len - 2] == ',')) {
            nonZeroCount++;
        }
    }
    ASSERT_NE(0, nonZeroCount);
    fclose(f);
}

void checkPerfStatCsv(const char *fileName) {
    FILE *f = fopen(fileName, "r");
    ASSERT_NE(nullptr, f);
    char line[4096];
    int nonZeroCount = 0;
    while (fgets(line, 4096, f) != nullptr) {
        int len = strlen(line);
        if (!(line[0] == '0' && line[1] == ',')) {
            nonZeroCount++;
        }
    }
    ASSERT_NE(0, nonZeroCount);
    fclose(f);
}