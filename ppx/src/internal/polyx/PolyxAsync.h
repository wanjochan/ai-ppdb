#ifndef POLYX_ASYNC_H
#define POLYX_ASYNC_H

#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxAsync.h"

// 高级异步操作封装，提供更友好的异步接口
// 基于 InfraxAsync 构建，添加更多高级特性

// 异步操作的状态
typedef enum {
    POLYX_ASYNC_PENDING,
    POLYX_ASYNC_SUCCESS,
    POLYX_ASYNC_ERROR
} PolyxAsyncStatus;

// 异步操作的结果
typedef struct {
    void* data;
    size_t size;
    int error_code;
    PolyxAsyncStatus status;
} PolyxAsyncResult;

// 异步操作的回调函数类型
typedef void (*PolyxAsyncCallback)(PolyxAsyncResult* result);

// 异步操作的基本结构
typedef struct PolyxAsync {
    InfraxAsync* infra;  // 底层异步实现
    PolyxAsyncResult* result;  // 异步操作结果
    void* private_data;  // 任务特定数据
    PolyxAsyncCallback callback;  // 完成回调
    
    // Instance methods
    struct PolyxAsync* (*start)(struct PolyxAsync* self);
    void (*cancel)(struct PolyxAsync* self);
    bool (*is_done)(struct PolyxAsync* self);
    PolyxAsyncResult* (*get_result)(struct PolyxAsync* self);
    void (*free)(struct PolyxAsync* self);
} PolyxAsync;

// 工厂方法类型
typedef struct {
    PolyxAsync* (*new)(void);
    void (*free)(PolyxAsync* self);
    PolyxAsync* (*read_file)(const char* path);
    PolyxAsync* (*write_file)(const char* path, const void* data, size_t size);
    PolyxAsync* (*http_get)(const char* url);
    PolyxAsync* (*http_post)(const char* url, const void* data, size_t size);
    PolyxAsync* (*delay)(int ms);
    PolyxAsync* (*interval)(int ms, int count);
    PolyxAsync* (*parallel)(PolyxAsync** tasks, int count);
    PolyxAsync* (*sequence)(PolyxAsync** tasks, int count);
} PolyxAsyncClassType;

// 全局静态类实例
extern const PolyxAsyncClassType PolyxAsyncClass;

// 构造和析构函数
PolyxAsync* polyx_async_new(void);
void polyx_async_free(PolyxAsync* self);

// 实例方法
PolyxAsync* polyx_async_start(PolyxAsync* self);
void polyx_async_cancel(PolyxAsync* self);
bool polyx_async_is_done(PolyxAsync* self);
PolyxAsyncResult* polyx_async_get_result(PolyxAsync* self);

// 工厂方法
PolyxAsync* polyx_async_read_file(const char* path);
PolyxAsync* polyx_async_write_file(const char* path, const void* data, size_t size);
PolyxAsync* polyx_async_http_get(const char* url);
PolyxAsync* polyx_async_http_post(const char* url, const void* data, size_t size);
PolyxAsync* polyx_async_delay(int ms);
PolyxAsync* polyx_async_interval(int ms, int count);
PolyxAsync* polyx_async_parallel(PolyxAsync** tasks, int count);
PolyxAsync* polyx_async_sequence(PolyxAsync** tasks, int count);

#endif // POLYX_ASYNC_H
