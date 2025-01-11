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
    memset(queue, 0, sizeof(infra_async_queue_t));
    infra_mutex_create(&queue->mutex);
    infra_cond_init(&queue->not_empty);
    infra_cond_init(&queue->not_full);
    infra_cond_init(&queue->task_completed);
}

static void queue_cleanup(infra_async_queue_t* queue) {
    if (queue == NULL) {
        return;
    }

    infra_mutex_lock(queue->mutex);
    
    // 清理所有节点
    infra_async_task_node_t* current = queue->head;
    while (current != NULL) {
        infra_async_task_node_t* next = current->next;
        memory_pool_free(current);
        current = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    memset(queue->priority_counts, 0, sizeof(queue->priority_counts));

    infra_mutex_unlock(queue->mutex);
    infra_mutex_destroy(queue->mutex);
    infra_cond_destroy(queue->not_empty);
    infra_cond_destroy(queue->not_full);
    infra_cond_destroy(queue->task_completed);
}

static infra_error_t queue_push(infra_async_queue_t* queue, infra_async_task_t* task) {
    if (queue == NULL || task == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    uint64_t start_time = infra_time_monotonic();
    infra_error_t err = infra_mutex_lock(queue->mutex);
    if (err != INFRA_OK) {
        return err;
    }

    // 检查队列是否已满
    while (queue->size >= queue->capacity) {
        err = infra_cond_wait(queue->not_full, queue->mutex);
        if (err != INFRA_OK) {
            infra_mutex_unlock(queue->mutex);
            return err;
        }
    }

    // 分配新节点
    infra_async_task_node_t* node = memory_pool_alloc();
    if (node == NULL) {
        infra_mutex_unlock(queue->mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化节点
    memcpy(&node->task, task, sizeof(infra_async_task_t));
    node->next = NULL;
    node->submit_time = infra_time_monotonic();
    node->start_time = 0;
    node->complete_time = 0;
    node->cancelled = false;

    // 插入队列
    if (queue->tail == NULL) {
        queue->head = node;
        queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }

    queue->size++;
    queue->priority_counts[task->priority]++;

    // 通知等待的消费者
    infra_cond_signal(queue->not_empty);

    // 更新锁统计
    update_lock_stats(&g_perf_stats.queue_lock, infra_time_monotonic() - start_time);

    infra_mutex_unlock(queue->mutex);
    return INFRA_OK;
}

static infra_error_t queue_pop(infra_async_queue_t* queue, infra_async_task_node_t** node) {
    if (queue == NULL || node == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    uint64_t start_time = infra_time_monotonic();
    infra_error_t err = infra_mutex_lock(queue->mutex);
    if (err != INFRA_OK) {
        return err;
    }

    // 等待队列非空
    while (queue->size == 0) {
        err = infra_cond_wait(queue->not_empty, queue->mutex);
        if (err != INFRA_OK) {
            infra_mutex_unlock(queue->mutex);
            return err;
        }
    }

    // 获取头节点
    *node = queue->head;
    queue->head = queue->head->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    queue->size--;
    queue->priority_counts[(*node)->task.priority]--;

    // 通知等待的生产者
    infra_cond_signal(queue->not_full);

    // 更新锁统计
    update_lock_stats(&g_perf_stats.queue_lock, infra_time_monotonic() - start_time);

    infra_mutex_unlock(queue->mutex);
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
    
    infra_error_t result = INFRA_OK;
    size_t bytes_processed = 0;
    
    switch (task->type) {
        case INFRA_ASYNC_READ: {
            size_t total_read = 0;
            while (total_read < task->io.size) {
                result = infra_file_read(task->io.fd, 
                                       (char*)task->io.buffer + total_read, 
                                       task->io.size - total_read, 
                                       &bytes_processed);
                if (result != INFRA_OK) break;
                if (bytes_processed == 0) {
                    result = INFRA_ERROR_IO;
                    break;
                }
                total_read += bytes_processed;
            }
            break;
        }
            
        case INFRA_ASYNC_WRITE: {
            size_t total_written = 0;
            while (total_written < task->io.size) {
                result = infra_file_write(task->io.fd, 
                                        (char*)task->io.buffer + total_written, 
                                        task->io.size - total_written, 
                                        &bytes_processed);
                if (result != INFRA_OK) break;
                if (bytes_processed == 0) {
                    result = INFRA_ERROR_IO;
                    break;
                }
                total_written += bytes_processed;
            }
            break;
        }
            
        case INFRA_ASYNC_EVENT:
            result = INFRA_OK;
            break;
            
        default:
            result = INFRA_ERROR_INVALID;
            break;
    }
    
    return result;
}

//-----------------------------------------------------------------------------
// Worker Thread
//-----------------------------------------------------------------------------

static void* worker_thread(void* arg) {
    infra_async_t* async = (infra_async_t*)arg;
    
    while (!async->stop) {
        infra_async_task_node_t* node = NULL;
        infra_error_t result = queue_pop(async->task_queue, &node);
        if (result != INFRA_OK) {
            continue;
        }

        // 更新任务状态
        node->start_time = infra_time_monotonic();
        uint64_t wait_time = node->start_time - node->submit_time;

        // 处理任务
        if (node->cancelled) {
            // 任务已被取消
            if (node->task.callback) {
                node->task.callback(&node->task, INFRA_ERROR_CANCELLED);
            }
        } else {
            // 执行任务
            result = process_task(&node->task);

            // 更新完成时间
            node->complete_time = infra_time_monotonic();
            uint64_t exec_time = node->complete_time - node->start_time;

            // 更新性能统计
            update_task_stats(&g_perf_stats.task, exec_time, wait_time);

            // 调用回调函数
            if (node->task.callback) {
                node->task.callback(&node->task, result);
            }

            // 更新完成任务计数
            __atomic_add_fetch(&async->task_queue->completed_tasks, 1, __ATOMIC_SEQ_CST);
            printf("Completed tasks: %lu\n", 
                   __atomic_load_n(&async->task_queue->completed_tasks, __ATOMIC_SEQ_CST));
        }

        // 通知等待的线程
        infra_mutex_lock(async->task_queue->mutex);
        infra_cond_broadcast(async->task_queue->task_completed);
        infra_mutex_unlock(async->task_queue->mutex);

        // 释放节点
        memory_pool_free(node);
    }

    return NULL;
}

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

infra_error_t infra_async_init(infra_async_t* async, const infra_config_t* config) {
    if (async == NULL || config == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (async->initialized) {
        return INFRA_OK;
    }

    // 初始化内存池
    infra_error_t err = memory_pool_init();
    if (err != INFRA_OK) {
        return err;
    }

    // 创建任务队列
    async->task_queue = infra_malloc(sizeof(infra_async_queue_t));
    if (async->task_queue == NULL) {
        memory_pool_cleanup();
        return INFRA_ERROR_NO_MEMORY;
    }

    queue_init(async->task_queue);
    async->task_queue->capacity = config->async.task_queue_size;

    // 创建互斥锁
    err = infra_mutex_create(&async->mutex);
    if (err != INFRA_OK) {
        queue_cleanup(async->task_queue);
        infra_free(async->task_queue);
        memory_pool_cleanup();
        return err;
    }

    // 创建工作线程
    async->num_threads = config->async.min_threads;
    async->worker_threads = infra_malloc(sizeof(infra_thread_t) * async->num_threads);
    if (async->worker_threads == NULL) {
        infra_mutex_destroy(async->mutex);
        queue_cleanup(async->task_queue);
        infra_free(async->task_queue);
        memory_pool_cleanup();
        return INFRA_ERROR_NO_MEMORY;
    }

    for (uint32_t i = 0; i < async->num_threads; i++) {
        err = infra_thread_create(&async->worker_threads[i], worker_thread, async);
        if (err != INFRA_OK) {
            // 清理已创建的线程
            for (uint32_t j = 0; j < i; j++) {
                infra_thread_join(async->worker_threads[j]);
            }
            infra_free(async->worker_threads);
            infra_mutex_destroy(async->mutex);
            queue_cleanup(async->task_queue);
            infra_free(async->task_queue);
            memory_pool_cleanup();
            return err;
        }
    }

    async->initialized = true;
    async->stop = false;

    // 重置性能统计
    infra_async_reset_perf_stats(async);

    return INFRA_OK;
}

void infra_async_cleanup(infra_async_t* async) {
    if (async == NULL || !async->initialized) {
        return;
    }

    // 设置停止标志
    async->stop = true;

    // 等待所有工作线程退出
    for (uint32_t i = 0; i < async->num_threads; i++) {
        infra_thread_join(async->worker_threads[i]);
    }

    // 清理资源
    infra_free(async->worker_threads);
    infra_mutex_destroy(async->mutex);
    queue_cleanup(async->task_queue);
    infra_free(async->task_queue);
    memory_pool_cleanup();

    // 重置状态
    memset(async, 0, sizeof(infra_async_t));
}

infra_error_t infra_async_submit(infra_async_t* async, infra_async_task_t* task) {
    if (!async || !async->initialized || !task) return INFRA_ERROR_INVALID;
    
    infra_mutex_lock(async->mutex);
    infra_error_t err = infra_list_append(async->task_queue, task);
    infra_mutex_unlock(async->mutex);
    
    return err;
}

infra_error_t infra_async_run(infra_async_t* async, uint32_t timeout_ms) {
    if (!async || !async->initialized) return INFRA_ERROR_INVALID;
    
    infra_time_t start_time = infra_time_monotonic();
    infra_error_t last_error = INFRA_OK;
    
    while (true) {
        infra_mutex_lock(async->mutex);
        infra_list_node_t* node = infra_list_head(async->task_queue);
        if (!node) {
            infra_mutex_unlock(async->mutex);
            break;
        }
        
        infra_async_task_t* task = (infra_async_task_t*)infra_list_node_value(node);
        infra_list_remove(async->task_queue, node);
        infra_mutex_unlock(async->mutex);
        
        // 处理任务
        infra_error_t err = process_task(task);
        if (task->callback) {
            task->callback(task, err);
        }
        if (err != INFRA_OK) {
            last_error = err;
        }
        
        // 检查超时
        if (timeout_ms > 0) {
            infra_time_t current_time = infra_time_monotonic();
            if (current_time - start_time >= timeout_ms) {
                break;
            }
        }
    }
    
    return last_error;
}

infra_error_t infra_async_cancel(infra_async_t* async, infra_async_task_t* task) {
    if (async == NULL || task == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_error_t result = INFRA_ERROR_NOT_FOUND;
    infra_mutex_lock(async->task_queue->mutex);

    // 遍历队列查找任务
    infra_async_task_node_t* current = async->task_queue->head;
    infra_async_task_node_t* prev = NULL;

    while (current != NULL) {
        if (memcmp(&current->task, task, sizeof(infra_async_task_t)) == 0) {
            // 找到任务，标记为已取消
            current->cancelled = true;
            result = INFRA_OK;

            // 如果任务还未开始执行，从队列中移除
            if (current->start_time == 0) {
                if (prev == NULL) {
                    async->task_queue->head = current->next;
                } else {
                    prev->next = current->next;
                }

                if (current == async->task_queue->tail) {
                    async->task_queue->tail = prev;
                }

                // 更新队列状态
                async->task_queue->size--;
                async->task_queue->priority_counts[current->task.priority]--;

                // 调用回调函数
                if (current->task.callback) {
                    current->task.callback(&current->task, INFRA_ERROR_CANCELLED);
                }

                // 通知等待的生产者
                infra_cond_signal(async->task_queue->not_full);

                // 释放节点
                memory_pool_free(current);
            } else if (current->start_time > 0 && !current->complete_time) {
                // 任务正在执行，等待其完成
                if (current->task.callback) {
                    current->task.callback(&current->task, INFRA_ERROR_CANCELLED);
                }
            }
            break;
        }
        prev = current;
        current = current->next;
    }

    infra_mutex_unlock(async->task_queue->mutex);
    return result;
}

infra_error_t infra_async_stop(infra_async_t* async) {
    if (!async || !async->initialized) return INFRA_ERROR_INVALID;
    return INFRA_OK;
}

void infra_async_destroy(infra_async_t* async) {
    if (!async || !async->initialized) return;
    
    infra_mutex_destroy(async->mutex);
    infra_list_destroy(async->task_queue);
    async->initialized = false;
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
    if (g_memory_pool.blocks != NULL) {
        return INFRA_OK;
    }

    infra_error_t err = infra_mutex_create(&g_memory_pool.mutex);
    if (err != INFRA_OK) {
        return err;
    }

    memory_block_t* block = infra_malloc(sizeof(memory_block_t));
    if (block == NULL) {
        infra_mutex_destroy(g_memory_pool.mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    memset(block, 0, sizeof(memory_block_t));
    g_memory_pool.blocks = block;
    g_memory_pool.total_nodes = MEMORY_POOL_BLOCK_SIZE;
    g_memory_pool.used_nodes = 0;

    return INFRA_OK;
}

static void memory_pool_cleanup(void) {
    if (g_memory_pool.blocks == NULL) {
        return;
    }

    memory_block_t* block = g_memory_pool.blocks;
    while (block != NULL) {
        memory_block_t* next = block->next;
        infra_free(block);
        block = next;
    }

    infra_mutex_destroy(g_memory_pool.mutex);
    memset(&g_memory_pool, 0, sizeof(memory_pool_t));
}

static infra_async_task_node_t* memory_pool_alloc(void) {
    uint64_t start_time = infra_time_monotonic();
    infra_error_t err = infra_mutex_lock(g_memory_pool.mutex);
    if (err != INFRA_OK) {
        return NULL;
    }

    memory_block_t* block = g_memory_pool.blocks;
    while (block != NULL) {
        for (size_t i = 0; i < MEMORY_POOL_BLOCK_SIZE; i++) {
            if (!block->used[i]) {
                block->used[i] = true;
                g_memory_pool.used_nodes++;
                infra_mutex_unlock(g_memory_pool.mutex);
                g_perf_stats.mempool.alloc_count++;
                g_perf_stats.mempool.alloc_time_us += infra_time_monotonic() - start_time;
                return &block->nodes[i];
            }
        }
        block = block->next;
    }

    // 需要分配新的内存块
    if (g_memory_pool.total_nodes >= MAX_MEMORY_BLOCKS * MEMORY_POOL_BLOCK_SIZE) {
        infra_mutex_unlock(g_memory_pool.mutex);
        return NULL;
    }

    block = infra_malloc(sizeof(memory_block_t));
    if (block == NULL) {
        infra_mutex_unlock(g_memory_pool.mutex);
        return NULL;
    }

    memset(block, 0, sizeof(memory_block_t));
    block->next = g_memory_pool.blocks;
    g_memory_pool.blocks = block;
    g_memory_pool.total_nodes += MEMORY_POOL_BLOCK_SIZE;
    block->used[0] = true;
    g_memory_pool.used_nodes++;

    infra_mutex_unlock(g_memory_pool.mutex);
    g_perf_stats.mempool.alloc_count++;
    g_perf_stats.mempool.alloc_time_us += infra_time_monotonic() - start_time;
    return &block->nodes[0];
}

static void memory_pool_free(infra_async_task_node_t* node) {
    uint64_t start_time = infra_time_monotonic();
    infra_error_t err = infra_mutex_lock(g_memory_pool.mutex);
    if (err != INFRA_OK) {
        return;
    }

    memory_block_t* block = g_memory_pool.blocks;
    while (block != NULL) {
        if ((node >= &block->nodes[0]) && (node < &block->nodes[MEMORY_POOL_BLOCK_SIZE])) {
            size_t index = node - &block->nodes[0];
            block->used[index] = false;
            g_memory_pool.used_nodes--;
            break;
        }
        block = block->next;
    }

    infra_mutex_unlock(g_memory_pool.mutex);
    g_perf_stats.mempool.free_count++;
    g_perf_stats.mempool.free_time_us += infra_time_monotonic() - start_time;
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
