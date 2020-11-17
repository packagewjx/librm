//
// Created by wjx on 2020/11/17.

extern "C"{
#include "../resource_manager.h"
#include <sys/user.h>
}
//
#include <gtest/gtest.h>

TEST(lib, init) {
    ASSERT_EQ(0, rm_init());
    ASSERT_EQ(0, rm_init());
    ASSERT_EQ(0, rm_init());
}

TEST(lib, fini) {
    ASSERT_EQ(0, rm_finalize());
    ASSERT_EQ(0, rm_finalize());
    ASSERT_EQ(0, rm_finalize());
}