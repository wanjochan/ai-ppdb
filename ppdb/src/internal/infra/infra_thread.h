#ifndef INFRA_THREAD_H
#define INFRA_THREAD_H

#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Thread Pool Types
//-----------------------------------------------------------------------------

// 线程池配置
typedef struct infra_thread_pool_config {
    int min_threads;     // 最小线程数
    int max_threads;     // 最大线程数
    int queue_size;      // 任务队列大小
    uint32_t idle_timeout;  // 空闲线程超时时间(ms)
} infra_thread_pool_config_t;

// 线程池句柄
typedef struct infra_thread_pool infra_thread_pool_t;

//-----------------------------------------------------------------------------
// Thread Pool Operations
//-----------------------------------------------------------------------------

// 创建线程池
infra_error_t infra_thread_pool_create(const infra_thread_pool_config_t* config,
                                     infra_thread_pool_t** pool);

// 销毁线程池
void infra_thread_pool_destroy(infra_thread_pool_t* pool);

// 提交任务
infra_error_t infra_thread_pool_submit(infra_thread_pool_t* pool,
                                     infra_thread_func_t fn,
                                     void* arg);

//-----------------------------------------------------------------------------
// Thread Pool Statistics
//-----------------------------------------------------------------------------

typedef struct infra_thread_pool_stats {
    size_t active_threads;   // 当前活动线程数
    size_t idle_threads;     // 当前空闲线程数
    size_t queued_tasks;     // 当前排队任务数
    size_t completed_tasks;  // 已完成任务总数
    size_t failed_tasks;     // 失败任务总数
} infra_thread_pool_stats_t;

// 获取线程池统计信息
infra_error_t infra_thread_pool_get_stats(infra_thread_pool_t* pool,
                                        infra_thread_pool_stats_t* stats);

#endif /* INFRA_THREAD_H */
