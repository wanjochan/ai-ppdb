/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_async.h - Unified Asynchronous System Interface
 */

#ifndef INFRA_ASYNC_H
#define INFRA_ASYNC_H

#include "infra.h"

//-----------------------------------------------------------------------------
// Async Configuration
//-----------------------------------------------------------------------------

// 任务处理方式
#define INFRA_PROCESS_AUTO     0  // 自动选择处理方式
#define INFRA_PROCESS_EVENTFD  1  // 使用eventfd处理（类似epoll）
#define INFRA_PROCESS_THREAD   2  // 使用线程池处理

//-----------------------------------------------------------------------------
// Async Types
//-----------------------------------------------------------------------------

// 原子计数器类型
typedef volatile size_t atomic_counter_t;

// 任务类型
typedef enum {
    INFRA_ASYNC_EVENT,  // 事件类型任务
    INFRA_ASYNC_READ,   // 读操作任务
    INFRA_ASYNC_WRITE   // 写操作任务
} infra_async_task_type_t;

// 任务分类
typedef enum {
    INFRA_TASK_TYPE_UNKNOWN = 0,  // 未知类型
    INFRA_TASK_TYPE_IO = 1,       // IO密集型
    INFRA_TASK_TYPE_CPU = 2       // CPU密集型
} infra_task_type_t;

// IO操作结构
typedef struct {
    int fd;           // 文件描述符
    void* buffer;     // 数据缓冲区
    size_t size;      // 数据大小
} infra_async_io_t;

// 任务配置结构
typedef struct {
    uint32_t io_threshold_us;    // IO任务判定阈值
    uint32_t cpu_threshold_us;   // CPU任务判定阈值
    uint32_t sample_window;      // 采样窗口大小
} infra_task_config_t;

// 任务性能分析
typedef struct {
    uint64_t last_exec_time;     // 最后执行时间
    uint32_t sample_count;       // 采样次数
    uint32_t io_ratio;          // IO比例
    uint32_t cpu_ratio;         // CPU比例
    infra_task_type_t type;     // 任务类型
    uint32_t process_method;    // 处理方式
} infra_task_profile_t;

// 任务优先级
typedef enum {
    INFRA_PRIORITY_LOW = 0,
    INFRA_PRIORITY_NORMAL = 1,
    INFRA_PRIORITY_HIGH = 2,
    INFRA_PRIORITY_CRITICAL = 3
} infra_task_priority_t;

// 任务结构
typedef struct infra_async_task {
    infra_async_task_type_t type;  // 任务类型
    infra_task_priority_t priority;  // 任务优先级
    union {
        infra_async_io_t io;       // IO操作数据
    };
    infra_task_profile_t profile;  // 任务性能分析
    void (*callback)(struct infra_async_task* task, infra_error_t result);  // 回调函数
} infra_async_task_t;

// 任务节点结构
typedef struct infra_async_task_node {
    uint32_t magic;                // 魔数，用于验证
    infra_async_task_t task;       // 任务
    struct infra_async_task_node* next;  // 下一个节点
    bool cancelled;                // 是否已取消
    infra_time_t submit_time;      // 提交时间
    infra_time_t start_time;       // 开始时间
    infra_time_t complete_time;    // 完成时间
} infra_async_task_node_t;

// 任务队列结构
typedef struct {
    infra_async_task_node_t* head;  // 队列头
    infra_async_task_node_t* tail;  // 队列尾
    size_t size;                    // 当前大小
    size_t max_size;                // 最大大小
    atomic_counter_t completed_tasks;  // 已完成任务计数（原子计数器）
    size_t priority_counts[4];      // 各优先级的任务数量
    void* mutex;                    // 互斥锁
    void* not_empty;                // 非空条件变量
    void* not_full;                 // 非满条件变量
    void* task_completed;           // 任务完成条件变量
} infra_async_queue_t;

// 工作线程结构
typedef struct {
    void* thread;                   // 线程句柄
    bool running;                   // 运行标志
    struct infra_async_context* ctx;  // 上下文
    uint32_t processed_tasks;        // 已处理任务数
    uint32_t failed_tasks;           // 失败任务数
} infra_async_worker_t;

// 异步上下文结构
typedef struct infra_async_context {
    bool initialized;               // 初始化标志
    bool running;                   // 运行标志
    bool stop_requested;            // 停止请求标志
    infra_async_queue_t queue;      // 任务队列
    infra_async_worker_t* workers;  // 工作线程数组
    uint32_t num_workers;           // 当前工作线程数
    uint32_t max_workers;           // 最大工作线程数
    void* mutex;                    // 互斥锁
    struct {
        uint32_t active_threads;     // 活动线程数
        uint32_t queued_tasks;       // 队列中的任务数
        uint32_t completed_tasks;    // 已完成任务数
        uint32_t failed_tasks;       // 失败任务数
        uint32_t timeout_tasks;      // 超时任务数
        uint32_t cancelled_tasks;    // 取消任务数
        uint64_t total_wait_time_us;  // 总等待时间
        uint64_t total_process_time_us;  // 总处理时间
        uint64_t max_wait_time_us;    // 最大等待时间
        uint64_t max_process_time_us;  // 最大处理时间
    } stats;
} infra_async_context_t;

// 异步统计结构
typedef struct {
    uint32_t active_threads;     // 活动线程数
    uint32_t queued_tasks;       // 队列中的任务数
    uint32_t completed_tasks;    // 已完成任务数
    uint32_t failed_tasks;       // 失败任务数
    uint32_t timeout_tasks;      // 超时任务数
    uint32_t cancelled_tasks;    // 取消任务数
    uint64_t total_wait_time_us;  // 总等待时间
    uint64_t total_process_time_us;  // 总处理时间
    uint64_t max_wait_time_us;    // 最大等待时间
    uint64_t max_process_time_us;  // 最大处理时间
} infra_async_stats_t;

// 异步处理器结构
typedef struct {
    infra_async_queue_t task_queue;    // 任务队列
    infra_thread_t worker;             // 工作线程
    bool stop;                         // 停止标志
    infra_config_t config;             // 配置信息
} infra_async_t;

// 性能分析数据结构
typedef struct {
    uint64_t lock_acquire_time_us;   // 获取锁的总时间
    uint64_t lock_wait_count;        // 等待锁的次数
    uint64_t lock_contention_count;  // 锁竞争次数
} infra_lock_stats_t;

typedef struct {
    size_t total_blocks;            // 总内存块数
    size_t total_nodes;            // 总节点数
    size_t used_nodes;             // 已使用节点数
    size_t peak_nodes;             // 峰值节点使用数
    uint64_t alloc_count;          // 分配次数
    uint64_t free_count;           // 释放次数
    uint64_t alloc_time_us;        // 分配总时间
    uint64_t free_time_us;         // 释放总时间
} infra_mempool_stats_t;

typedef struct {
    uint64_t task_count;           // 任务总数
    uint64_t total_exec_time_us;   // 总执行时间
    uint64_t min_exec_time_us;     // 最短执行时间
    uint64_t max_exec_time_us;     // 最长执行时间
    uint64_t avg_exec_time_us;     // 平均执行时间
    uint64_t total_wait_time_us;   // 总等待时间
    uint64_t min_wait_time_us;     // 最短等待时间
    uint64_t max_wait_time_us;     // 最长等待时间
    uint64_t avg_wait_time_us;     // 平均等待时间
} infra_task_stats_t;

typedef struct {
    infra_lock_stats_t queue_lock;      // 队列锁统计
    infra_lock_stats_t mempool_lock;    // 内存池锁统计
    infra_mempool_stats_t mempool;      // 内存池统计
    infra_task_stats_t task;            // 任务统计
    uint64_t start_time;                // 统计开始时间
    uint64_t update_time;               // 最后更新时间
} infra_perf_stats_t;

//-----------------------------------------------------------------------------
// Function Declarations
//-----------------------------------------------------------------------------

// 异步系统函数
infra_error_t infra_async_init(infra_async_t* async, const infra_config_t* config);
void infra_async_cleanup(infra_async_t* async);
infra_error_t infra_async_submit(infra_async_t* async, infra_async_task_t* task);
infra_error_t infra_async_run(infra_async_t* async, uint32_t timeout_ms);
infra_error_t infra_async_cancel(infra_async_t* async, infra_async_task_t* task);
infra_error_t infra_async_stop(infra_async_t* async);
void infra_async_destroy(infra_async_t* async);
infra_error_t infra_async_get_stats(infra_async_t* async, infra_async_stats_t* stats);
infra_error_t infra_async_get_perf_stats(infra_async_t* async, infra_perf_stats_t* stats);
infra_error_t infra_async_reset_perf_stats(infra_async_t* async);
infra_error_t infra_async_export_perf_stats(infra_async_t* async, const char* filename);

#endif // INFRA_ASYNC_H 