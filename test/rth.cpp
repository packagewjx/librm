//
// Created by wjx on 2020/11/26.
//

#include "gtest/gtest.h"

extern "C" {
#include "../rth.h"
}

TEST(rth, sample) {
    // Case 1
    const int size = 20;
    uint64_t addr1[size] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    struct rm_mem_mon_trace_data records[size];
    struct rm_mem_rth_context data{};
    ASSERT_EQ(0, rm_mem_rth_start(size, &data));
    for (int i = 0; i < size; i++) {
        records[i].addr = addr1[i];
    }

    ASSERT_EQ(0, rm_mem_rth_update(&data, records, size));
    ASSERT_EQ(size, hashmap_num_entries(&data.reservoir));

    rm_mem_rth *result;
    ASSERT_EQ(0, rm_mem_rth_finish(&data, &result, size));
    ASSERT_EQ(size, result->occurrence[0]);
    rm_mem_rth_destroy(result);

    // Case 2
    uint64_t addr2[size] = {1, 1, 2, 20, 2, 3, 7, 8, 3, 4, 11, 12, 13, 4, 5, 16, 17, 18, 19, 5};
    ASSERT_EQ(0, rm_mem_rth_start(size, &data));
    for (int i = 0; i < size; i++) {
        records[i].addr = addr2[i];
    }
    ASSERT_EQ(0, rm_mem_rth_update(&data, records, size));
    ASSERT_EQ(15, hashmap_num_entries(&data.reservoir));
    ASSERT_EQ(0, rm_mem_rth_finish(&data, &result, size));
    EXPECT_EQ(10, result->occurrence[0]);
    EXPECT_EQ(1, result->occurrence[1]);
    EXPECT_EQ(1, result->occurrence[2]);
    EXPECT_EQ(1, result->occurrence[3]);
    EXPECT_EQ(1, result->occurrence[4]);
    EXPECT_EQ(1, result->occurrence[5]);
    rm_mem_rth_destroy(result);

    // Case 3 随机用例
    const int largeSize = 1000;
    rm_mem_mon_trace_data largeRecords[largeSize * 10];
    ASSERT_EQ(0, rm_mem_rth_start(largeSize, &data));
    for (int i = 0; i < largeSize * 10; i++) {
        largeRecords[i].addr = random() & 0xFFFFF;
    }
    ASSERT_EQ(0, rm_mem_rth_update(&data, largeRecords, largeSize * 10));
    ASSERT_EQ(0, rm_mem_rth_finish(&data, &result, 100));
    ASSERT_EQ(100, result->maxTime);
    int total = 0;
    for (int i = 0; i <= 100; i++) {
        total += result->occurrence[i];
    }
    ASSERT_NE(0, total);
    rm_mem_rth_destroy(result);
}
