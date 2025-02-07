#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

// 异步函数类型
typedef void (*AsyncFn)(void* arg);

//状态重复了
// 状态定义
#define ASYNC_STATE_INIT    0  // 初始状态
#define ASYNC_STATE_RUNNING 1  // 正在运行
#define ASYNC_STATE_YIELD   2  // 已让出控制权
#define ASYNC_STATE_DONE    3  // 已完成
#define ASYNC_STATE_ERROR   4  // 出错

// 任务状态枚举
typedef enum {
    INFRAX_ASYNC_INIT = 0,
    INFRAX_ASYNC_RUNNING,
    INFRAX_ASYNC_DONE,
    INFRAX_ASYNC_ERROR
} InfraxAsyncStatus;

// 异步任务结构体（不透明类型）
typedef struct InfraxAsync InfraxAsync;

// 启动异步任务
InfraxAsync* infrax_async_start(AsyncFn fn, void* arg);

// 等待任务完成
// int infrax_async_wait(InfraxAsync* async);

// 让出控制权
void infrax_async_yield(InfraxAsync* async);

// 非阻塞地获取任务状态
InfraxAsyncStatus infrax_async_status(InfraxAsync* async);

#endif // INFRAX_ASYNC_H
