/**
 * @file InfraxAsync.h
 * @brief Async coroutine functionality for the infrax subsystem
 */

#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

#include "InfraxCore.h"
#include <setjmp.h>
#include <stdbool.h>

// IO事件类型
#define INFRAX_IO_READ  (1 << 0)
#define INFRAX_IO_WRITE (1 << 1)
#define INFRAX_IO_ERROR (1 << 2)

// 栈布局相关宏
#define INFRAX_STACK_ALIGN     16
#define INFRAX_STACK_REDZONE   128
#define INFRAX_MIN_STACK_SIZE  1024
#define DEFAULT_STACK_SIZE     (8 * 1024)  // 默认栈大小为8KB

// 协程状态
typedef enum {
    COROUTINE_INIT = 0,      // 初始状态
    COROUTINE_READY = 1,     // 就绪状态
    COROUTINE_RUNNING = 2,   // 运行状态
    COROUTINE_YIELDED = 3,   // 已让出状态
    COROUTINE_DONE = 4       // 完成状态
} InfraxAsyncState;

// 异步操作类型
typedef enum {
    ASYNC_NONE = 0,
    ASYNC_TIMER,      // 定时器
    ASYNC_IO,         // IO操作
    ASYNC_EVENT,      // 事件等待
    ASYNC_RESOURCE,   // 资源等待
    ASYNC_BATCH       // 批处理
} InfraxAsyncType;

// 前向声明
typedef struct InfraxAsync InfraxAsync;
typedef struct InfraxAsyncClass InfraxAsyncClass;

// 协程配置
typedef struct {
    const char* name;         // 协程名称
    void (*fn)(void*);       // 协程函数
    void* arg;               // 函数参数
    size_t stack_size;       // 栈大小
} InfraxAsyncConfig;

// 协程实例方法
struct InfraxAsync {
    const InfraxAsyncClass* klass;
    InfraxAsyncConfig config;
    InfraxAsyncState state;
    InfraxAsync* next;
    jmp_buf env;
    
    // 协程栈
    void* stack;             // 栈空间
    void* raw_stack;         // 原始栈指针(用于释放)
    size_t stack_size;       // 栈大小
    void* stack_top;         // 栈顶指针
    
    // 异步操作类型和参数
    InfraxAsyncType type;
    union {
        struct {
            int fd;
            int events;
        } io;
        struct {
            int ms;
            uint64_t start_time;  // 定时器开始时间
        } timer;
    } params;
    
    // 实例方法
    InfraxError (*start)(InfraxAsync* self);
    InfraxError (*yield)(InfraxAsync* self);
    InfraxError (*resume)(InfraxAsync* self);
    bool (*is_done)(const InfraxAsync* self);
};

// 类方法
struct InfraxAsyncClass {
    InfraxAsync* (*new)(const InfraxAsyncConfig* config);
    void (*free)(InfraxAsync* self);
};

// 全局函数
void InfraxAsyncRun(void);

// 工厂函数
InfraxAsync* InfraxAsync_CreateTimer(int ms);
InfraxAsync* InfraxAsync_CreateIO(int fd, int events);
InfraxAsync* InfraxAsync_CreateEvent(void* event_source);
InfraxAsync* InfraxAsync_CreateResource(void* resource);
InfraxAsync* InfraxAsync_CreateBatch(void* items, size_t count);

// 类实例
extern const InfraxAsyncClass InfraxAsync_CLASS;

#endif // INFRAX_ASYNC_H
