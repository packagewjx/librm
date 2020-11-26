//
// Created by wjx on 2020/11/25.

extern "C" {
#include "../perfmem.h"
#include "../log/src/log.h"
extern struct rm_mem_mon_trace_data *read_perf_data(const char *name, int *recordLen);
}

#include <cstdlib>
#include "gtest/gtest.h"

TEST(perfmem, readrecord) {
    system("sudo perf record -o test.data -e cpu/mem-loads/p,cpu/mem-stores/p -d -p 1 sleep 1");
    system("sudo chmod +r test.data");
    int len;
    rm_mem_mon_trace_data *data = read_perf_data("test.data", &len);
    for (int i = 0; i < len; i++) {
        ASSERT_NE(0, data->addr);
    }
    system("sudo rm test.data");
}

TEST(perfmem, mem_trace) {
    log_set_level(LOG_DEBUG);
    rm_mem_mon_data group{};
    pid_t pid = getpid();
    ASSERT_EQ(0, rm_mem_mon_start(&pid, 1, &group));
    sleep(1);
    for (int i = 0; i < 3; i++) {
        sleep(1);
        rm_mem_mon_trace_data *record = nullptr;
        int len;
        rm_mem_mon_poll(&group, &len, &record);
        int numZero = 0;
        for (int j = 0; j < len; j++) {
            if (record[j].addr == 0) {
                numZero++;
            }
        }
        ASSERT_FALSE(numZero > 0 && numZero == len);
        free(record);
    }
    ASSERT_EQ(0, rm_mem_mon_stop(&group));
}