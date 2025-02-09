#ifndef POLYX_ASYNC_H
#define POLYX_ASYNC_H

#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxAsync.h"

// 高级异步操作封装，提供更友好的异步接口
// 基于 InfraxAsync 构建，添加更多高级特性

typedef struct PolyxAsync PolyxAsync;
// typedef struct PolyxAsyncClass PolyxAsyncClass;

// 异步操作的结果
typedef struct {
    void* data;
    size_t size;
    int error_code;
} PolyxAsyncResult;

// 异步操作的状态回调
typedef void (*PolyxAsyncCallback)(PolyxAsync* self, PolyxAsyncResult* result);

// 实例结构
struct PolyxAsync {
    InfraxAsync* infra;  // 底层异步实现
    PolyxAsyncResult* result;  // 异步操作结果
    PolyxAsyncCallback on_complete;  // 完成回调
    PolyxAsyncCallback on_error;     // 错误回调
    PolyxAsyncCallback on_progress;  // 进度回调
    
    // 实例方法
    PolyxAsync* (*start)(PolyxAsync* self);
    void (*cancel)(PolyxAsync* self);
    bool (*is_done)(PolyxAsync* self);
    PolyxAsyncResult* (*get_result)(PolyxAsync* self);
};

PolyxAsync* polyx_async_new(void);
void polyx_async_free(PolyxAsync* self);
PolyxAsync* polyx_async_read_file(const char* path);
PolyxAsync* polyx_async_write_file(const char* path, const void* data, size_t size);
PolyxAsync* polyx_async_http_get(const char* url);
PolyxAsync* polyx_async_http_post(const char* url, const void* data, size_t size);
PolyxAsync* polyx_async_delay(int ms);
PolyxAsync* polyx_async_interval(int ms, int count);
PolyxAsync* polyx_async_parallel(PolyxAsync** tasks, int count);
PolyxAsync* polyx_async_sequence(PolyxAsync** tasks, int count);

// 类结构
typedef struct {
    // 构造和析构
    PolyxAsync* (*new)(void);
    void (*free)(PolyxAsync* self);
    
    // 工厂方法 - 文件操作
    PolyxAsync* (*read_file)(const char* path);
    PolyxAsync* (*write_file)(const char* path, const void* data, size_t size);
    
    // 工厂方法 - 网络操作
    PolyxAsync* (*http_get)(const char* url);
    PolyxAsync* (*http_post)(const char* url, const void* data, size_t size);
    
    // 工厂方法 - 定时器
    PolyxAsync* (*delay)(int ms);
    PolyxAsync* (*interval)(int ms, int count);
    
    // 工厂方法 - 并发
    PolyxAsync* (*parallel)(PolyxAsync** tasks, int count);
    PolyxAsync* (*sequence)(PolyxAsync** tasks, int count);
} PolyxAsyncClassType;

// 全局静态类实例
static PolyxAsyncClassType PolyxAsyncClass = {
    .new = polyx_async_new,
    .free = polyx_async_free,
    .read_file = polyx_async_read_file,
    .write_file = polyx_async_write_file,
    .http_get = polyx_async_http_get,
    .http_post = polyx_async_http_post,
    .delay = polyx_async_delay,
    .interval = polyx_async_interval,
    .parallel = polyx_async_parallel,
    .sequence = polyx_async_sequence
};

#endif // POLYX_ASYNC_H
