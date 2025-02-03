#ifndef INFRA_ASYNC_H
#define INFRA_ASYNC_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_thread.h"

// 栈大小配置
#define INFRA_STACK_MIN  (4 * 1024)     // 4KB 起始栈
#define INFRA_STACK_MAX  (1024 * 1024)  // 1MB 上限

// 协程函数类型
typedef void (*infra_async_fn)(void*);

// 协程上下文
typedef struct infra_async_ctx {
    jmp_buf env;           // 上下文
    char* stack;           // 栈指针
    size_t size;          // 当前栈大小
    size_t used;          // 已用空间
    infra_async_fn fn;     // 协程函数
    void* arg;            // 函数参数
    int done;             // 是否完成
    struct infra_async_ctx* next;  // 链表指针
} infra_async_ctx;

// 调度器统计
typedef struct infra_scheduler_stats {
    size_t total_allocs;   // 总分配次数
    size_t grow_count;     // 栈增长次数
    size_t peak_size;      // 峰值大小
    size_t total_steals;   // 窃取次数
    size_t total_yields;   // 让出次数
} infra_scheduler_stats;

// 调度器结构
typedef struct infra_scheduler {
    infra_async_ctx* ready;     // 就绪队列
    infra_async_ctx* current;   // 当前协程
    jmp_buf env;               // 调度器上下文
    infra_scheduler_stats stats;// 统计信息
    void* user_data;           // 用户数据
    int id;                    // 调度器ID
} infra_scheduler_t;

// 基础协程API
infra_async_ctx* infra_go(infra_async_fn fn, void* arg);  // 创建协程（默认调度器）
infra_async_ctx* infra_go_in(infra_scheduler_t* sched,    // 创建协程（指定调度器）
                            infra_async_fn fn, void* arg);
// infra_yield 由 infra_sync.h 提供
void infra_run(void);                                     // 运行默认调度器
void infra_run_in(infra_scheduler_t* sched);             // 运行指定调度器

// 内存管理API
void* infra_alloc(size_t size);                          // 在当前协程栈上分配内存
void infra_reset(void);                                  // 重置当前协程的内存使用

// 调度器管理API
infra_scheduler_t* infra_scheduler_create(int id);        // 创建新调度器
void infra_scheduler_destroy(infra_scheduler_t* sched);   // 销毁调度器
infra_scheduler_t* infra_scheduler_current(void);         // 获取当前线程的调度器
void infra_scheduler_set_current(infra_scheduler_t* s);   // 设置当前线程的调度器
bool infra_scheduler_steal(infra_scheduler_t* from,       // 窃取任务
                         infra_scheduler_t* to);

// 获取当前协程上下文
infra_async_ctx* infra_current(void);

#endif /* INFRA_ASYNC_H */