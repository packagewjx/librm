//
// Created by wjx on 2020/11/11.
//

extern "C" {
#include "../utils/container.h"
}

#include <cstdlib>
#include <gtest/gtest.h>

TEST(stack, pushpop) {
    struct Stack stack{};
    stackInit(&stack);
    int data[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        stackPush(&stack, &data[i]);
    }

    EXPECT_EQ(5, stack.len);
    EXPECT_LT(5, stack.cap);
    for (int i = 0; i < stack.len; i++) {
        EXPECT_EQ(*(int *) stack.arr[i], data[i]);
    }

    for (int i = 0; i < stack.len; i++) {
        EXPECT_EQ(*(int *) stackPop(&stack), data[4 - i]);
    }
}

TEST(stack, enlarge) {
    struct Stack stack{};
    stackInit(&stack);

    const int Size = 100;
    int *testData = (int *) malloc(Size * sizeof(int));
    for (int i = 0; i < Size; i++) {
        testData[i] = i;
        stackPush(&stack, &testData[i]);
    }

    EXPECT_GT(stack.cap, stack.len);
    EXPECT_EQ(Size, stack.len);
    for (int i = 0; i < Size; i++) {
        EXPECT_EQ(testData[Size - 1 - i], *(int*)stackPop(&stack));
    }
}

TEST(stack, push_pop_sequence) {
    int data[] = {1, 2, 3, 4, 5};
    struct Stack stack{};
    stackInit(&stack);

    stackPush(&stack, &data[0]);
    stackPush(&stack, &data[1]);
    stackPush(&stack, &data[2]);

    EXPECT_EQ(3, *(int*)stackPop(&stack));
    stackPush(&stack, &data[3]);
    EXPECT_EQ(4, *(int*)stackPop(&stack));
    EXPECT_EQ(2, *(int*)stackPop(&stack));
    stackPush(&stack, &data[4]);
    EXPECT_EQ(5, *(int*)stackPop(&stack));
    EXPECT_EQ(1, *(int*)stackPop(&stack));
}