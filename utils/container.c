//
// Created by wjx on 2020/11/11.
//

#include "container.h"
#include <stdlib.h>
#include <string.h>

#define STACK_DEFAULT_CAPACITY 16


void stackInit(struct Stack *stack) {
    stack->arr = malloc(STACK_DEFAULT_CAPACITY * sizeof(void *));
    stack->len = 0;
    stack->cap = STACK_DEFAULT_CAPACITY;
}

void stackPush(struct Stack *stack, void *item) {
    if (stack->len == stack->cap) {
        // 扩容
        int newCap = stack->len * 2;
        stack->arr = realloc(stack->arr, newCap * sizeof(void *));
        stack->cap = newCap;
    }

    stack->arr[stack->len++] = item;
}

void *stackPop(struct Stack *stack) {
    if (stack->len == 0) {
        return NULL;
    }

    void *item = stack->arr[stack->len - 1];
    stack->len--;
    return item;
}

void stackDestroy(struct Stack *stack) {
    free(stack->arr);
}


