/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_async.c - Unified Asynchronous System Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_async.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// External References
//-----------------------------------------------------------------------------

extern struct {
    bool initialized;
    infra_init_flags_t active_flags;
    infra_config_t config;
    infra_status_t status;
    infra_mutex_t mutex;
} g_infra;

//-----------------------------------------------------------------------------
// Memory Pool
//-----------------------------------------------------------------------------

#define MEMORY_POOL_BLOCK_SIZE 32  // 每个内存块可以容纳的节点数
#define MAX_MEMORY_BLOCKS 32       // 最大内存块数
#define INFRA_ASYNC_TASK_MAGIC 0xA5A5A5A5

typedef struct memory_block {
    infra_async_task_node_t nodes[MEMORY_POOL_BLOCK_SIZE];
    bool used[MEMORY_POOL_BLOCK_SIZE];
    struct memory_block* next;
} memory_block_t;

typedef struct {
    memory_block_t* blocks;
    size_t total_nodes;
    size_t used_nodes;
    infra_mutex_t mutex;
} memory_pool_t;

static memory_pool_t g_memory_pool = {0};
static infra_perf_stats_t g_perf_stats = {0};

// 前向声明
static void update_lock_stats(infra_lock_stats_t* stats, uint64_t acquire_time);
static void update_task_stats(infra_task_stats_t* stats, uint64_t exec_time, uint64_t wait_time);
static infra_error_t memory_pool_init(void);
static void memory_pool_cleanup(void);
static infra_async_task_node_t* memory_pool_alloc(void);
static void memory_pool_free(infra_async_task_node_t* node);
static infra_async_task_node_t* queue_node_alloc(void);
static void queue_node_free(infra_async_task_node_t* node);

//-----------------------------------------------------------------------------
// Queue Operations
//-----------------------------------------------------------------------------

static void queue_init(infra_async_queue_t* queue) {
    if (!queue) return;
    
    // 初始化队列
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->max_size = g_infra.config.async.task_queue_size > 0 ? g_infra.config.async.task_queue_size : 16;
    queue->completed_tasks = 0;
    memset(queue->priority_counts, 0, sizeof(queue->priority_counts));
    
    // 初始化互斥锁和条件变量
    infra_platform_mutex_create(&queue->mutex);
    infra_platform_cond_create(&queue->not_empty);
    infra_platform_cond_create(&queue->not_full);
    infra_platform_cond_create(&queue->task_completed);
}

static void queue_cleanup(infra_async_queue_t* queue) {
    if (!queue) return;
    
    // 清理所有节点
    infra_async_task_node_t* current = queue->head;
    while (current) {
        infra_async_task_node_t* next = current->next;
        free(current);
        current = next;
    }
    
    // 销毁互斥锁和条件变量
    infra_platform_mutex_destroy(queue->mutex);
    infra_platform_cond_destroy(queue->not_empty);
    infra_platform_cond_destroy(queue->not_full);
    infra_platform_cond_destroy(queue->task_completed);  // 销毁任务完成条件变量
    
    // 重置队列状态
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->completed_tasks = 0;  // 重置已完成任务计数
}

static infra_error_t queue_push(infra_async_queue_t* queue, infra_async_task_t* task) {
    if (!queue || !task) return INFRA_ERROR_INVALID;
    
    printf("Pushing task to queue with priority %d\n", task->priority);
    
    // 分配节点
    infra_async_task_node_t* node = queue_node_alloc();
    if (!node) return INFRA_ERROR_NOMEM;
    
    // 初始化节点
    node->magic = INFRA_ASYNC_TASK_MAGIC;
    node->task = *task;
    node->next = NULL;
    node->cancelled = false;
    node->submit_time = infra_time_monotonic();
    node->start_time = 0;
    node->complete_time = 0;
    
    // 加锁并记录时间
    uint64_t lock_start = infra_time_monotonic();
    infra_platform_mutex_lock(queue->mutex);
    update_lock_stats(&g_perf_stats.queue_lock, infra_time_monotonic() - lock_start);
    
    // 检查队列是否已满
    if (queue->size >= queue->max_size) {
        printf("Queue is full (size=%zu, max=%zu), waiting...\n", queue->size, queue->max_size);
        infra_platform_cond_wait(queue->not_full, queue->mutex);
    }
    
    // 根据优先级插入队列
    if (!queue->head || task->priority > queue->head->task.priority) {
        // 插入队列头部
        node->next = queue->head;
        queue->head = node;
        if (!queue->tail) {
            queue->tail = node;
        }
    } else {
        // 在队列中查找合适的位置
        infra_async_task_node_t* current = queue->head;
        while (current->next && current->next->task.priority >= task->priority) {
            current = current->next;
        }
        node->next = current->next;
        current->next = node;
        if (!node->next) {
            queue->tail = node;
        }
    }
    
    queue->size++;
    queue->priority_counts[task->priority]++;
    
    printf("Task pushed to queue, size=%zu, priority_count[%d]=%zu\n", 
           queue->size, task->priority, queue->priority_counts[task->priority]);
    
    // 发送信号
    infra_platform_cond_signal(queue->not_empty);
    
    // 解锁
    infra_platform_mutex_unlock(queue->mutex);
    
    return INFRA_OK;
}

static infra_error_t queue_pop(infra_async_queue_t* queue, infra_async_task_node_t** node) {
    if (!queue || !node) return INFRA_ERROR_INVALID;
    
    *node = NULL;  // 初始化返回值
    
    // 加锁
    infra_platform_mutex_lock(queue->mutex);
    
    // 等待队列非空
    while (queue->size == 0) {
        printf("Queue is empty, waiting for signal...\n");
        // 等待有新任务，最多等待1秒
        infra_error_t err = infra_platform_cond_timedwait(queue->not_empty, queue->mutex, 1000);
        if (err == INFRA_ERROR_TIMEOUT) {
            printf("Wait timed out\n");
            infra_platform_mutex_unlock(queue->mutex);
            return INFRA_ERROR_TIMEOUT;
        }
        printf("Woke up, queue size=%zu\n", queue->size);
    }
    
    // 从队列头部取出节点
    infra_async_task_node_t* popped = queue->head;
    queue->head = popped->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    
    queue->size--;
    queue->priority_counts[popped->task.priority]--;
    
    printf("Task popped from queue, size=%zu, priority_count[%d]=%zu\n", 
           queue->size, popped->task.priority, queue->priority_counts[popped->task.priority]);
    
    // 发送信号
    infra_platform_cond_signal(queue->not_full);
    
    // 解锁
    infra_platform_mutex_unlock(queue->mutex);
    
    // 在锁外设置返回值
    *node = popped;
    
    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Task Type Analysis
//-----------------------------------------------------------------------------

static void update_task_profile(infra_async_task_t* task, uint64_t exec_time) {
    if (!task) return;
    
    // 更新执行时间
    task->profile.last_exec_time = exec_time;
    task->profile.sample_count++;
    
    // 基于任务类型的初始判断
    if (task->type == INFRA_ASYNC_READ || task->type == INFRA_ASYNC_WRITE) {
        task->profile.io_ratio = (task->profile.io_ratio * (task->profile.sample_count - 1) + 100) 
                                / task->profile.sample_count;
        task->profile.cpu_ratio = 100 - task->profile.io_ratio;
        // IO操作默认使用eventfd处理
        task->profile.process_method = INFRA_PROCESS_EVENTFD;
    } else {
        // 对于EVENT类型，我们通过执行时间来判断
        if (exec_time > g_infra.config.async.classify.cpu_threshold_us) {
            task->profile.cpu_ratio = (task->profile.cpu_ratio * (task->profile.sample_count - 1) + 100) 
                                    / task->profile.sample_count;
            task->profile.io_ratio = 100 - task->profile.cpu_ratio;
            // CPU密集型任务使用线程池处理
            task->profile.process_method = INFRA_PROCESS_THREAD;
        } else if (exec_time < g_infra.config.async.classify.io_threshold_us) {
            task->profile.io_ratio = (task->profile.io_ratio * (task->profile.sample_count - 1) + 70) 
                                    / task->profile.sample_count;
            task->profile.cpu_ratio = 100 - task->profile.io_ratio;
            // IO密集型任务使用eventfd处理
            task->profile.process_method = INFRA_PROCESS_EVENTFD;
        }
    }
    
    // 根据比例确定任务类型
    if (task->profile.io_ratio > 60) {
        task->profile.type = INFRA_TASK_TYPE_IO;
    } else if (task->profile.cpu_ratio > 60) {
        task->profile.type = INFRA_TASK_TYPE_CPU;
    } else {
        task->profile.type = INFRA_TASK_TYPE_UNKNOWN;
    }
}

//-----------------------------------------------------------------------------
// Task Processing Methods
//-----------------------------------------------------------------------------

static infra_error_t process_task(infra_async_task_t* task) {
    if (!task) return INFRA_ERROR_INVALID;
    
    printf("Processing task of type %d...\n", task->type);
    infra_time_t start_time = infra_time_monotonic();
    infra_error_t result = INFRA_OK;
    size_t bytes_processed = 0;
    
    switch (task->type) {
        case INFRA_ASYNC_READ:
            printf("Processing READ task...\n");
            result = infra_file_read(task->io.fd, task->io.buffer, task->io.size, &bytes_processed);
            break;
            
        case INFRA_ASYNC_WRITE:
            printf("Processing WRITE task...\n");
            result = infra_file_write(task->io.fd, task->io.buffer, task->io.size, &bytes_processed);
            break;
            
        case INFRA_ASYNC_EVENT:
            printf("Processing EVENT task...\n");
            // 事件类型任务直接返回成功
            result = INFRA_OK;
            break;
            
        default:
            printf("Unknown task type: %d\n", task->type);
            result = INFRA_ERROR_INVALID;
            break;
    }
    
    // 更新任务性能分析
    update_task_profile(task, infra_time_monotonic() - start_time);
    printf("Task processing completed with result: %d\n", result);
    
    return result;
}

//-----------------------------------------------------------------------------
// Worker Thread
//-----------------------------------------------------------------------------

static void* worker_thread(void* arg) {
    infra_async_t* async = (infra_async_t*)arg;
    if (!async) return NULL;
    
    printf("Worker thread started\n");
    
    while (!async->stop) {
        printf("Worker thread waiting for task...\n");
        infra_async_task_node_t* node = NULL;
        infra_error_t result = queue_pop(&async->task_queue, &node);
        
        if (result == INFRA_ERROR_TIMEOUT) {
            printf("Queue is empty, continuing...\n");
            infra_time_sleep(1);  // 避免空转
            continue;
        }
        
        if (result != INFRA_OK || !node) {
            printf("Error popping task: %d\n", result);
            continue;
        }
        
        printf("Processing task...\n");
        
        // 记录开始时间
        node->start_time = infra_time_monotonic();
        uint64_t wait_time = node->start_time - node->submit_time;
        
        // 检查任务是否被取消
        if (node->cancelled) {
            printf("Task was cancelled before processing\n");
            if (node->task.callback) {
                node->task.callback(&node->task, INFRA_ERROR_CANCELLED);
            }
        } else {
            // 处理任务
            result = process_task(&node->task);
            
            // 记录完成时间和执行时间
            node->complete_time = infra_time_monotonic();
            uint64_t exec_time = node->complete_time - node->start_time;
            
            // 更新任务统计
            update_task_stats(&g_perf_stats.task, exec_time, wait_time);
            
            // 调用回调函数
            if (node->task.callback) {
                printf("Calling task callback with result: %d\n", result);
                node->task.callback(&node->task, result);
            }
            
            // 原子递增完成任务计数
            __atomic_add_fetch(&async->task_queue.completed_tasks, 1, __ATOMIC_SEQ_CST);
            printf("Task completed, total completed: %zu\n", 
                   __atomic_load_n(&async->task_queue.completed_tasks, __ATOMIC_SEQ_CST));
        }
        
        // 发送任务完成信号
        infra_platform_mutex_lock(async->task_queue.mutex);
        infra_platform_cond_broadcast(async->task_queue.task_completed);
        infra_platform_mutex_unlock(async->task_queue.mutex);
        
        // 释放节点
        memory_pool_free(node);
    }
    
    printf("Worker thread stopped\n");
    return NULL;
}

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

infra_error_t infra_async_init(infra_async_t* async, const infra_config_t* config) {
    if (!async || !config) return INFRA_ERROR_INVALID;
    
    // 初始化内存池
    infra_error_t err = memory_pool_init();
    if (err != INFRA_OK) return err;
    
    // 初始化成员
    memset(async, 0, sizeof(infra_async_t));
    async->stop = false;  // 初始状态为运行
    
    // 保存配置
    memcpy(&async->config, config, sizeof(infra_config_t));
    
    // 创建任务队列
    queue_init(&async->task_queue);
    
    // 启动工作线程
    err = infra_thread_create(&async->worker, worker_thread, async);
    if (err != INFRA_OK) {
        queue_cleanup(&async->task_queue);
        memory_pool_cleanup();  // 清理内存池
        return err;
    }
    
    // 重置性能统计
    memset(&g_perf_stats, 0, sizeof(g_perf_stats));
    g_perf_stats.start_time = infra_time_monotonic();
    g_perf_stats.update_time = g_perf_stats.start_time;
    
    return INFRA_OK;
}

void infra_async_cleanup(infra_async_t* async) {
    if (!async) return;
    
    // 停止工作线程
    if (!async->stop) {
        infra_async_stop(async);
    }
    
    // 清理任务队列
    queue_cleanup(&async->task_queue);
    
    // 清理内存池
    memory_pool_cleanup();
}

infra_error_t infra_async_submit(infra_async_t* async, infra_async_task_t* task) {
    if (!async || !task) return INFRA_ERROR_INVALID;
    
    // 检查是否已停止（使用原子操作）
    if (async->stop) {
        printf("Async system is stopped\n");
        return INFRA_ERROR_STATE;
    }
    
    // 设置默认优先级（在加锁之前完成）
    if (task->priority > INFRA_PRIORITY_CRITICAL) {
        printf("Invalid priority %d, using default priority (NORMAL)\n", task->priority);
        task->priority = INFRA_PRIORITY_NORMAL;
    }
    
    // 根据任务类型调整优先级（在加锁之前完成）
    if (task->type == INFRA_ASYNC_READ || task->type == INFRA_ASYNC_WRITE) {
        if (task->priority == INFRA_PRIORITY_LOW) {
            task->priority = INFRA_PRIORITY_NORMAL;
        }
    }
    
    printf("Submitting task with type=%d, priority=%d\n", task->type, task->priority);
    
    // 检查队列是否已满（使用单独的锁范围）
    infra_platform_mutex_lock(async->task_queue.mutex);
    bool is_full = (async->task_queue.size >= async->task_queue.max_size);
    infra_platform_mutex_unlock(async->task_queue.mutex);
    
    if (is_full) {
        printf("Queue is full\n");
        return INFRA_ERROR_FULL;
    }
    
    return queue_push(&async->task_queue, task);
}

infra_error_t infra_async_run(infra_async_t* async, uint32_t timeout_ms) {
    if (!async) return INFRA_ERROR_INVALID;
    
    printf("Waiting for tasks to complete, timeout=%ums\n", timeout_ms);
    
    // 等待任务完成或超时
    infra_time_t start_time = infra_time_monotonic();
    
    while (!async->stop) {
        // 检查是否超时
        infra_time_t current_time = infra_time_monotonic();
        if (timeout_ms > 0 && current_time - start_time >= timeout_ms * 1000) {  // 转换为微秒
            printf("Run timed out\n");
            return INFRA_ERROR_TIMEOUT;
        }
        
        // 检查任务是否都已完成
        infra_platform_mutex_lock(async->task_queue.mutex);
        
        printf("Checking task status: queue_size=%zu, completed_tasks=%zu\n", 
               async->task_queue.size, async->task_queue.completed_tasks);
        
        // 如果队列为空且所有任务都已完成，则返回
        if (async->task_queue.size == 0 && async->task_queue.completed_tasks > 0) {
            printf("All tasks completed\n");
            infra_platform_mutex_unlock(async->task_queue.mutex);
            return INFRA_OK;
        }
        
        // 等待任务完成信号
        if (async->task_queue.size > 0) {
            // 计算剩余超时时间
            uint64_t elapsed_ms = (current_time - start_time) / 1000;  // 转换为毫秒
            uint64_t remaining_ms = timeout_ms > elapsed_ms ? timeout_ms - elapsed_ms : 1;
            
            printf("Waiting for task completion signal, remaining=%lums\n", remaining_ms);
            
            // 等待任务完成信号
            infra_error_t err = infra_platform_cond_timedwait(
                async->task_queue.task_completed,
                async->task_queue.mutex,
                remaining_ms
            );
            
            if (err == INFRA_ERROR_TIMEOUT) {
                printf("Wait timed out\n");
                infra_platform_mutex_unlock(async->task_queue.mutex);
                return INFRA_ERROR_TIMEOUT;
            }
            
            printf("Got task completion signal\n");
        }
        
        infra_platform_mutex_unlock(async->task_queue.mutex);
    }
    
    return INFRA_OK;
}

infra_error_t infra_async_cancel(infra_async_t* async, infra_async_task_t* task) {
    if (!async || !task) return INFRA_ERROR_INVALID;
    
    printf("Attempting to cancel task...\n");
    infra_error_t result = INFRA_ERROR_NOTFOUND;
    
    infra_platform_mutex_lock(async->task_queue.mutex);
    
    // 遍历队列查找任务
    infra_async_task_node_t* current = async->task_queue.head;
    infra_async_task_node_t* prev = NULL;
    
    while (current) {
        if (memcmp(&current->task, task, sizeof(infra_async_task_t)) == 0) {
            // 标记任务为已取消
            current->cancelled = true;
            printf("Task marked as cancelled\n");
            
            // 如果任务还没开始执行，从队列中移除
            if (current->start_time == 0) {
                if (prev) {
                    prev->next = current->next;
                } else {
                    async->task_queue.head = current->next;
                }
                
                if (current == async->task_queue.tail) {
                    async->task_queue.tail = prev;
                }
                
                // 更新队列状态
                async->task_queue.size--;
                async->task_queue.priority_counts[current->task.priority]--;
                
                // 通知任务已取消
                if (current->task.callback) {
                    current->task.callback(&current->task, INFRA_ERROR_CANCELLED);
                }
                
                // 释放节点
                memory_pool_free(current);
                printf("Task removed from queue\n");
                
                // 发送信号表示队列有空位
                infra_platform_cond_signal(async->task_queue.not_full);
            } else if (current->start_time > 0 && !current->complete_time) {
                // 任务已经开始执行但还没完成，只能标记为取消
                printf("Task is already running, marked for cancellation\n");
            }
            
            result = INFRA_OK;
            break;
        }
        prev = current;
        current = current->next;
    }
    
    infra_platform_mutex_unlock(async->task_queue.mutex);
    
    if (result != INFRA_OK) {
        printf("Task not found in queue\n");
    }
    
    return result;
}

infra_error_t infra_async_stop(infra_async_t* async) {
    if (!async) return INFRA_ERROR_INVALID;
    
    // 设置停止标志
    async->stop = true;
    
    // 等待工作线程结束
    if (async->worker) {
        infra_thread_join(async->worker);
        async->worker = NULL;
    }
    
    return INFRA_OK;
}

void infra_async_destroy(infra_async_t* async) {
    if (!async) return;
    
    // 确保已停止
    if (!async->stop) {
        infra_async_stop(async);
    }
    
    // 销毁任务队列
    queue_cleanup(&async->task_queue);
}

infra_error_t infra_async_get_stats(infra_async_t* async, infra_async_stats_t* stats) {
    if (!async || !stats) return INFRA_ERROR_INVALID;
    
    // 清空统计信息
    memset(stats, 0, sizeof(infra_async_stats_t));
    
    // 获取队列状态
    infra_platform_mutex_lock(async->task_queue.mutex);
    
    stats->queued_tasks = async->task_queue.size;
    stats->completed_tasks = async->task_queue.completed_tasks;
    
    // 遍历队列计算等待时间
    infra_async_task_node_t* current = async->task_queue.head;
    infra_time_t now = infra_time_monotonic();
    
    while (current) {
        if (current->start_time == 0) {
            // 任务还在等待
            uint64_t wait_time = now - current->submit_time;
            stats->total_wait_time_us += wait_time;
            if (wait_time > stats->max_wait_time_us) {
                stats->max_wait_time_us = wait_time;
            }
        } else if (current->complete_time > 0) {
            // 任务已完成
            uint64_t process_time = current->complete_time - current->start_time;
            stats->total_process_time_us += process_time;
            if (process_time > stats->max_process_time_us) {
                stats->max_process_time_us = process_time;
            }
        }
        current = current->next;
    }
    
    infra_platform_mutex_unlock(async->task_queue.mutex);
    
    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Performance Statistics
//-----------------------------------------------------------------------------

static void update_lock_stats(infra_lock_stats_t* stats, uint64_t acquire_time) {
    stats->lock_acquire_time_us += acquire_time;
    stats->lock_wait_count++;
    if (acquire_time > 1000) {  // 如果获取锁时间超过1ms，认为发生了竞争
        stats->lock_contention_count++;
    }
}

static void update_task_stats(infra_task_stats_t* stats, uint64_t exec_time, uint64_t wait_time) {
    stats->task_count++;
    stats->total_exec_time_us += exec_time;
    stats->total_wait_time_us += wait_time;
    
    // 更新执行时间统计
    if (exec_time < stats->min_exec_time_us || stats->min_exec_time_us == 0) {
        stats->min_exec_time_us = exec_time;
    }
    if (exec_time > stats->max_exec_time_us) {
        stats->max_exec_time_us = exec_time;
    }
    stats->avg_exec_time_us = stats->total_exec_time_us / stats->task_count;
    
    // 更新等待时间统计
    if (wait_time < stats->min_wait_time_us || stats->min_wait_time_us == 0) {
        stats->min_wait_time_us = wait_time;
    }
    if (wait_time > stats->max_wait_time_us) {
        stats->max_wait_time_us = wait_time;
    }
    stats->avg_wait_time_us = stats->total_wait_time_us / stats->task_count;
}

infra_error_t infra_async_get_perf_stats(infra_async_t* async, infra_perf_stats_t* stats) {
    if (!async || !stats) return INFRA_ERROR_INVALID;
    
    // 更新内存池统计
    g_perf_stats.mempool.total_blocks = g_memory_pool.blocks ? 1 : 0;
    memory_block_t* current = g_memory_pool.blocks;
    while (current && current->next) {
        g_perf_stats.mempool.total_blocks++;
        current = current->next;
    }
    g_perf_stats.mempool.total_nodes = g_memory_pool.total_nodes;
    g_perf_stats.mempool.used_nodes = g_memory_pool.used_nodes;
    if (g_memory_pool.used_nodes > g_perf_stats.mempool.peak_nodes) {
        g_perf_stats.mempool.peak_nodes = g_memory_pool.used_nodes;
    }
    
    // 更新时间戳
    g_perf_stats.update_time = infra_time_monotonic();
    
    // 复制统计数据
    memcpy(stats, &g_perf_stats, sizeof(infra_perf_stats_t));
    
    return INFRA_OK;
}

infra_error_t infra_async_reset_perf_stats(infra_async_t* async) {
    if (!async) return INFRA_ERROR_INVALID;
    
    memset(&g_perf_stats, 0, sizeof(g_perf_stats));
    g_perf_stats.start_time = infra_time_monotonic();
    g_perf_stats.update_time = g_perf_stats.start_time;
    
    return INFRA_OK;
}

infra_error_t infra_async_export_perf_stats(infra_async_t* async, const char* filename) {
    if (!async || !filename) return INFRA_ERROR_INVALID;
    
    FILE* file = fopen(filename, "w");
    if (!file) return INFRA_ERROR_IO;
    
    // 获取最新统计数据
    infra_perf_stats_t stats;
    infra_error_t err = infra_async_get_perf_stats(async, &stats);
    if (err != INFRA_OK) {
        fclose(file);
        return err;
    }
    
    // 写入统计数据
    fprintf(file, "Async System Performance Statistics\n");
    fprintf(file, "==================================\n\n");
    
    fprintf(file, "Time Information:\n");
    fprintf(file, "- Start time: %lu us\n", stats.start_time);
    fprintf(file, "- Update time: %lu us\n", stats.update_time);
    fprintf(file, "- Running time: %lu us\n\n", stats.update_time - stats.start_time);
    
    fprintf(file, "Task Statistics:\n");
    fprintf(file, "- Total tasks: %lu\n", stats.task.task_count);
    fprintf(file, "- Average execution time: %lu us\n", stats.task.avg_exec_time_us);
    fprintf(file, "- Min execution time: %lu us\n", stats.task.min_exec_time_us);
    fprintf(file, "- Max execution time: %lu us\n", stats.task.max_exec_time_us);
    fprintf(file, "- Average wait time: %lu us\n", stats.task.avg_wait_time_us);
    fprintf(file, "- Min wait time: %lu us\n", stats.task.min_wait_time_us);
    fprintf(file, "- Max wait time: %lu us\n\n", stats.task.max_wait_time_us);
    
    fprintf(file, "Lock Statistics:\n");
    fprintf(file, "Queue Lock:\n");
    fprintf(file, "- Total acquire time: %lu us\n", stats.queue_lock.lock_acquire_time_us);
    fprintf(file, "- Wait count: %lu\n", stats.queue_lock.lock_wait_count);
    fprintf(file, "- Contention count: %lu\n\n", stats.queue_lock.lock_contention_count);
    
    fprintf(file, "Memory Pool Lock:\n");
    fprintf(file, "- Total acquire time: %lu us\n", stats.mempool_lock.lock_acquire_time_us);
    fprintf(file, "- Wait count: %lu\n", stats.mempool_lock.lock_wait_count);
    fprintf(file, "- Contention count: %lu\n\n", stats.mempool_lock.lock_contention_count);
    
    fprintf(file, "Memory Pool Statistics:\n");
    fprintf(file, "- Total blocks: %zu\n", stats.mempool.total_blocks);
    fprintf(file, "- Total nodes: %zu\n", stats.mempool.total_nodes);
    fprintf(file, "- Used nodes: %zu\n", stats.mempool.used_nodes);
    fprintf(file, "- Peak nodes: %zu\n", stats.mempool.peak_nodes);
    fprintf(file, "- Allocation count: %lu\n", stats.mempool.alloc_count);
    fprintf(file, "- Free count: %lu\n", stats.mempool.free_count);
    fprintf(file, "- Average allocation time: %lu us\n", 
            stats.mempool.alloc_count ? stats.mempool.alloc_time_us / stats.mempool.alloc_count : 0);
    fprintf(file, "- Average free time: %lu us\n", 
            stats.mempool.free_count ? stats.mempool.free_time_us / stats.mempool.free_count : 0);
    
    fclose(file);
    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Memory Pool
//-----------------------------------------------------------------------------

static infra_error_t memory_pool_init(void) {
    if (g_memory_pool.blocks) return INFRA_OK;  // 已经初始化
    
    infra_error_t err = infra_platform_mutex_create(&g_memory_pool.mutex);
    if (err != INFRA_OK) return err;
    
    g_memory_pool.total_nodes = 0;
    g_memory_pool.used_nodes = 0;
    
    return INFRA_OK;
}

static void memory_pool_cleanup(void) {
    infra_platform_mutex_lock(g_memory_pool.mutex);
    
    memory_block_t* block = g_memory_pool.blocks;
    while (block) {
        memory_block_t* next = block->next;
        free(block);
        block = next;
    }
    
    g_memory_pool.blocks = NULL;
    g_memory_pool.total_nodes = 0;
    g_memory_pool.used_nodes = 0;
    
    infra_platform_mutex_unlock(g_memory_pool.mutex);
    infra_platform_mutex_destroy(g_memory_pool.mutex);
}

static infra_async_task_node_t* memory_pool_alloc(void) {
    infra_time_t start_time = infra_time_monotonic();
    infra_platform_mutex_lock(g_memory_pool.mutex);
    
    // 遍历现有块，查找空闲节点
    memory_block_t* block = g_memory_pool.blocks;
    while (block) {
        for (size_t i = 0; i < MEMORY_POOL_BLOCK_SIZE; i++) {
            if (!block->used[i]) {
                block->used[i] = true;
                g_memory_pool.used_nodes++;
                
                // 更新性能统计
                g_perf_stats.mempool.alloc_count++;
                g_perf_stats.mempool.alloc_time_us += infra_time_monotonic() - start_time;
                
                infra_platform_mutex_unlock(g_memory_pool.mutex);
                return &block->nodes[i];
            }
        }
        block = block->next;
    }
    
    // 没有空闲节点，创建新块
    if (g_memory_pool.total_nodes >= MAX_MEMORY_BLOCKS * MEMORY_POOL_BLOCK_SIZE) {
        infra_platform_mutex_unlock(g_memory_pool.mutex);
        return NULL;  // 达到最大限制
    }
    
    block = (memory_block_t*)malloc(sizeof(memory_block_t));
    if (!block) {
        infra_platform_mutex_unlock(g_memory_pool.mutex);
        return NULL;
    }
    
    memset(block, 0, sizeof(memory_block_t));
    block->next = g_memory_pool.blocks;
    g_memory_pool.blocks = block;
    
    // 使用第一个节点
    block->used[0] = true;
    g_memory_pool.total_nodes += MEMORY_POOL_BLOCK_SIZE;
    g_memory_pool.used_nodes++;
    
    // 更新性能统计
    g_perf_stats.mempool.alloc_count++;
    g_perf_stats.mempool.alloc_time_us += infra_time_monotonic() - start_time;
    
    infra_platform_mutex_unlock(g_memory_pool.mutex);
    return &block->nodes[0];
}

static void memory_pool_free(infra_async_task_node_t* node) {
    if (!node) return;
    
    infra_time_t start_time = infra_time_monotonic();
    infra_platform_mutex_lock(g_memory_pool.mutex);
    
    // 查找节点所在的块
    memory_block_t* block = g_memory_pool.blocks;
    while (block) {
        if ((node >= block->nodes) && 
            (node < block->nodes + MEMORY_POOL_BLOCK_SIZE)) {
            // 找到对应的块
            size_t index = node - block->nodes;
            if (block->used[index]) {
                block->used[index] = false;
                g_memory_pool.used_nodes--;
                
                // 更新性能统计
                g_perf_stats.mempool.free_count++;
                g_perf_stats.mempool.free_time_us += infra_time_monotonic() - start_time;
            }
            break;
        }
        block = block->next;
    }
    
    infra_platform_mutex_unlock(g_memory_pool.mutex);
}

static infra_async_task_node_t* queue_node_alloc(void) {
    // 从内存池分配节点
    uint64_t alloc_start = infra_time_monotonic();
    infra_async_task_node_t* node = memory_pool_alloc();
    uint64_t alloc_time = infra_time_monotonic() - alloc_start;
    
    if (node) {
        // 更新性能统计
        infra_platform_mutex_lock(g_memory_pool.mutex);
        g_perf_stats.mempool.alloc_time_us += alloc_time;
        g_perf_stats.mempool.alloc_count++;
        infra_platform_mutex_unlock(g_memory_pool.mutex);
    }
    
    return node;
}

static void queue_node_free(infra_async_task_node_t* node) {
    if (!node) return;
    
    // 释放节点到内存池
    uint64_t free_start = infra_time_monotonic();
    memory_pool_free(node);
    uint64_t free_time = infra_time_monotonic() - free_start;
    
    // 更新性能统计
    infra_platform_mutex_lock(g_memory_pool.mutex);
    g_perf_stats.mempool.free_time_us += free_time;
    g_perf_stats.mempool.free_count++;
    infra_platform_mutex_unlock(g_memory_pool.mutex);
}
