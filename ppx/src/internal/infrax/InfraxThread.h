/**
 * @file InfraxThread.h
 * @brief Thread management functionality for the infrax subsystem
 */

#ifndef INFRAX_THREAD_H
#define INFRAX_THREAD_H

#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxSync.h"
#include "internal/infrax/InfraxMemory.h"

// 错误码定义
#define INFRAX_ERROR_THREAD_OK                INFRAX_ERROR_OK
#define INFRAX_ERROR_THREAD_INVALID_ARGUMENT  -201
#define INFRAX_ERROR_THREAD_CREATE_FAILED     -202
#define INFRAX_ERROR_THREAD_JOIN_FAILED       -203
#define INFRAX_ERROR_THREAD_ALREADY_RUNNING   -204
#define INFRAX_ERROR_THREAD_NOT_RUNNING       -205

// 线程池配置
typedef struct {
    int min_threads;     // 最小线程数
    int max_threads;     // 最大线程数
    int queue_size;      // 任务队列大小
    int idle_timeout;    // 空闲线程超时时间(ms)
} InfraxThreadPoolConfig;

// 线程池统计信息
typedef struct {
    int active_threads;  // 当前活动线程数
    int idle_threads;    // 当前空闲线程数
    int pending_tasks;   // 等待执行的任务数
    int completed_tasks; // 已完成的任务数
} InfraxThreadPoolStats;

// 线程函数类型
typedef void* (*InfraxThreadFunc)(void*);

// 线程ID类型
typedef uint64_t InfraxThreadId;

// 线程配置
typedef struct {
    const char* name;           // 线程名称
    InfraxThreadFunc func;      // 线程函数
    void* arg;                  // 线程参数
    size_t stack_size;         // 栈大小(可选)
    int priority;              // 优先级(可选)
} InfraxThreadConfig;

// 线程结构体
typedef struct InfraxThread {
    void* native_handle;       // 底层线程句柄
    void* private_data;        // 私有数据
    InfraxThreadConfig config; // 线程配置
    bool is_running;          // 运行状态
    void* result;             // 线程返回值
    InfraxSync* mutex;        // 状态保护
    
    // 实例方法
    InfraxError (*start)(struct InfraxThread* self, InfraxThreadFunc func, void* arg);
    InfraxError (*join)(struct InfraxThread* self, void** result);
    InfraxThreadId (*tid)(struct InfraxThread* self);
} InfraxThread;

// 线程类型
typedef struct {
    InfraxThread* (*new)(InfraxThreadConfig* config);
    void (*free)(InfraxThread* self);
    InfraxError (*start)(InfraxThread* self, InfraxThreadFunc func, void* arg);
    InfraxError (*join)(InfraxThread* self, void** result);
    InfraxThreadId (*tid)(InfraxThread* self);
    InfraxError (*pool_create)(InfraxThread* self, InfraxThreadPoolConfig* config);
    InfraxError (*pool_destroy)(InfraxThread* self);
    InfraxError (*pool_submit)(InfraxThread* self, InfraxThreadFunc func, void* arg);
    InfraxError (*pool_get_stats)(InfraxThread* self, InfraxThreadPoolStats* stats);
} InfraxThreadClassType;

// The "static" interface instance (like Java's Class object)
extern const InfraxThreadClassType InfraxThreadClass;

// // Memory manager function 不要暴露！
// InfraxMemory* get_memory_manager(void);

#endif /* INFRAX_THREAD_H */
