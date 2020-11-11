//
// Created by wjx on 2020/11/11.
//

#ifndef RESOURCEMANAGER_CONTAINER_H

struct Stack {
    void **arr;
    int len;
    int cap;
};

void stackInit(struct Stack* stack);

void stackPush(struct Stack* stack, void* item);

void* stackPop(struct Stack* stack);

#define RESOURCEMANAGER_CONTAINER_H

#endif //RESOURCEMANAGER_CONTAINER_H
