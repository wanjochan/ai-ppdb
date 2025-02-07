#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

// 异步函数类型
typedef void (*AsyncFn)(void* arg);

// 异步任务结构体（不透明类型）
typedef struct InfraxAsync InfraxAsync;

// 启动异步任务
InfraxAsync* infrax_async_start(AsyncFn fn, void* arg);

// 等待任务完成
int infrax_async_wait(InfraxAsync* async);

// 让出控制权
void infrax_async_yield(InfraxAsync* async);

#endif // INFRAX_ASYNC_H
