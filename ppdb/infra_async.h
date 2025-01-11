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

// 异步任务类型
typedef enum {
    INFRA_ASYNC_READ,
    INFRA_ASYNC_WRITE,
    INFRA_ASYNC_EVENT
} infra_async_type_t;

// 任务优先级
typedef enum {
    INFRA_PRIORITY_LOW,
    INFRA_PRIORITY_NORMAL,
    INFRA_PRIORITY_HIGH,
    INFRA_PRIORITY_CRITICAL,
    INFRA_PRIORITY_COUNT
} infra_priority_t;

// 任务类型
typedef enum {
    INFRA_TASK_TYPE_UNKNOWN,
    INFRA_TASK_TYPE_IO,
    INFRA_TASK_TYPE_CPU,
    INFRA_TASK_TYPE_COUNT
} infra_task_type_t;

// 任务处理方法
typedef enum {
    INFRA_PROCESS_UNKNOWN,
    INFRA_PROCESS_THREAD,
    INFRA_PROCESS_EVENTFD,
    INFRA_PROCESS_COUNT
} infra_process_method_t;

// 任务性能分析
typedef struct {
    uint64_t last_exec_time;
    uint32_t sample_count;
    uint32_t io_ratio;
    uint32_t cpu_ratio;
    infra_task_type_t type;
    infra_process_method_t process_method;
} infra_task_profile_t;

// 异步任务结构
typedef struct infra_async_task {
    infra_async_type_t type;
    infra_priority_t priority;
    void (*callback)(struct infra_async_task* task, infra_error_t error);
    struct {
        infra_handle_t fd;
        void* buffer;
        size_t size;
    } io;
    infra_task_profile_t profile;
} infra_async_task_t;

// 任务节点
typedef struct infra_async_task_node {
    infra_async_task_t task;
    struct infra_async_task_node* next;
    uint64_t submit_time;
    uint64_t start_time;
    uint64_t complete_time;
    bool cancelled;
} infra_async_task_node_t;

// 锁性能统计
typedef struct {
    uint64_t lock_acquire_time_us;
    uint64_t lock_wait_count;
    uint64_t lock_contention_count;
} infra_lock_stats_t;

// 任务性能统计
typedef struct {
    uint64_t task_count;
    uint64_t total_exec_time_us;
    uint64_t avg_exec_time_us;
    uint64_t min_exec_time_us;
    uint64_t max_exec_time_us;
    uint64_t total_wait_time_us;
    uint64_t avg_wait_time_us;
    uint64_t min_wait_time_us;
    uint64_t max_wait_time_us;
} infra_task_stats_t;

// 内存池性能统计
typedef struct {
    size_t total_blocks;
    size_t total_nodes;
    size_t used_nodes;
    size_t peak_nodes;
    uint64_t alloc_count;
    uint64_t free_count;
    uint64_t alloc_time_us;
    uint64_t free_time_us;
} infra_mempool_stats_t;

// 整体性能统计
typedef struct {
    uint64_t start_time;
    uint64_t update_time;
    infra_task_stats_t task;
    infra_lock_stats_t queue_lock;
    infra_lock_stats_t mempool_lock;
    infra_mempool_stats_t mempool;
} infra_perf_stats_t;

// 异步任务统计
typedef struct {
    uint64_t total_tasks;
    uint64_t queued_tasks;
    uint64_t completed_tasks;
    uint64_t cancelled_tasks;
    uint64_t failed_tasks;
    uint64_t avg_queue_size;
    uint64_t peak_queue_size;
    uint64_t total_wait_time_us;
    uint64_t max_wait_time_us;
    uint64_t total_process_time_us;
    uint64_t max_process_time_us;
} infra_async_stats_t;

// 异步任务队列
typedef struct {
    infra_async_task_node_t* head;
    infra_async_task_node_t* tail;
    size_t size;
    size_t capacity;
    uint32_t priority_counts[INFRA_PRIORITY_COUNT];
    infra_mutex_t mutex;
    infra_cond_t not_empty;
    infra_cond_t not_full;
    infra_cond_t task_completed;
    uint64_t completed_tasks;
} infra_async_queue_t;

// 异步系统结构
typedef struct infra_async {
    bool initialized;
    bool stop;
    infra_async_queue_t* task_queue;
    infra_mutex_t mutex;
    infra_thread_t* worker_threads;
    uint32_t num_threads;
} infra_async_t;

// 核心API
infra_error_t infra_async_init(infra_async_t* async, const infra_config_t* config);
void infra_async_destroy(infra_async_t* async);
void infra_async_cleanup(infra_async_t* async);
infra_error_t infra_async_submit(infra_async_t* async, infra_async_task_t* task);
infra_error_t infra_async_run(infra_async_t* async, uint32_t timeout_ms);
infra_error_t infra_async_stop(infra_async_t* async);
infra_error_t infra_async_cancel(infra_async_t* async, infra_async_task_t* task);

// 性能统计API
infra_error_t infra_async_get_stats(infra_async_t* async, infra_async_stats_t* stats);
infra_error_t infra_async_get_perf_stats(infra_async_t* async, infra_perf_stats_t* stats);
infra_error_t infra_async_reset_perf_stats(infra_async_t* async);
infra_error_t infra_async_export_perf_stats(infra_async_t* async, const char* filename);

// 文件操作API
infra_error_t infra_file_sync(infra_handle_t handle);

#endif /* INFRA_ASYNC_H */ 