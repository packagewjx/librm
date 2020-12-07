//
// Created by wjx on 2020/12/6.
//

#include <gtest/gtest.h>
#include <sys/time.h>

extern "C" {
#include "../utils/example_programs.h"
#include "../utils/general.h"
}


TEST(example_program, sequenceMemoryAccessor) {
    unsigned long t = getCurrentTimeMilli();
    sequenceMemoryAccessor();
    t = getCurrentTimeMilli() - t;
    printf("%ld\n", t);
    ASSERT_GT(t, 3000);
}