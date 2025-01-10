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
// Constants
//-----------------------------------------------------------------------------

#define INFRA_ASYNC_TASK_MAGIC 0xA5A5A5A5

//-----------------------------------------------------------------------------
// Queue Operations
//-----------------------------------------------------------------------------

static bool queue_is_empty(infra_async_queue_t* queue) {
    if (!queue) return true;
    
    // 加锁保护
    infra_platform_mutex_lock(queue->mutex);
    bool is_empty = (queue->size == 0);
    infra_platform_mutex_unlock(queue->mutex);
    
    return is_empty;
}

static void queue_init(infra_async_queue_t* queue) {
    if (!queue) return;
    
    // 初始化队列
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->max_size = g_infra.config.async.task_queue_size;
    
    // 初始化互斥锁和条件变量
    infra_platform_mutex_create(&queue->mutex);
    infra_platform_cond_create(&queue->not_empty);
    infra_platform_cond_create(&queue->not_full);
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
    
    // 重置队列状态
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
}

static infra_error_t queue_push(infra_async_queue_t* queue, infra_async_task_t* task) {
    if (!queue || !task) return INFRA_ERROR_INVALID;
    
    // 创建新节点
    infra_async_task_node_t* node = (infra_async_task_node_t*)malloc(sizeof(infra_async_task_node_t));
    if (!node) return INFRA_ERROR_NOMEM;
    
    // 初始化节点
    node->magic = INFRA_ASYNC_TASK_MAGIC;
    node->task = *task;
    node->next = NULL;
    node->cancelled = false;
    node->submit_time = infra_time_monotonic();
    node->start_time = 0;
    node->complete_time = 0;
    
    // 加锁
    infra_platform_mutex_lock(queue->mutex);
    
    // 检查队列是否已满
    while (queue->size >= queue->max_size) {
        infra_platform_cond_wait(queue->not_full, queue->mutex);
    }
    
    // 添加到队列尾部
    if (queue->tail) {
        queue->tail->next = node;
        queue->tail = node;
    } else {
        queue->head = queue->tail = node;
    }
    queue->size++;
    
    // 发送信号
    infra_platform_cond_signal(queue->not_empty);
    
    // 解锁
    infra_platform_mutex_unlock(queue->mutex);
    
    return INFRA_OK;
}

static infra_error_t queue_pop(infra_async_queue_t* queue, infra_async_task_node_t** node) {
    if (!queue || !node) return INFRA_ERROR_INVALID;
    
    // 加锁
    infra_platform_mutex_lock(queue->mutex);
    
    // 等待队列非空
    while (queue->size == 0) {
        // 等待有新任务
        infra_platform_cond_wait(queue->not_empty, queue->mutex);
        // 如果线程被唤醒但队列仍为空，继续等待
    }
    
    // 从队列头部取出节点
    *node = queue->head;
    
    queue->head = (*node)->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;
    
    // 发送信号
    infra_platform_cond_signal(queue->not_full);
    
    // 解锁
    infra_platform_mutex_unlock(queue->mutex);
    
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
    
    infra_time_t start_time = infra_time_monotonic();
    infra_error_t result = INFRA_OK;
    
    switch (task->type) {
        case INFRA_ASYNC_READ:
            // 暂时返回成功，因为还没实现真正的读操作
            result = INFRA_OK;
            break;
            
        case INFRA_ASYNC_WRITE:
            // 暂时返回成功，因为还没实现真正的写操作
            result = INFRA_OK;
            break;
            
        case INFRA_ASYNC_EVENT:
            // 事件类型任务直接返回成功
            result = INFRA_OK;
            break;
            
        default:
            result = INFRA_ERROR_INVALID;
            break;
    }
    
    // 更新任务性能分析
    update_task_profile(task, infra_time_monotonic() - start_time);
    
    return result;
}

//-----------------------------------------------------------------------------
// Worker Thread
//-----------------------------------------------------------------------------

static void* worker_thread(void* arg) {
    infra_async_t* async = (infra_async_t*)arg;
    if (!async) return NULL;
    
    while (!async->stop) {
        infra_async_task_node_t* node = NULL;
        infra_error_t result = queue_pop(&async->task_queue, &node);
        
        if (result != INFRA_OK || !node) {
            continue;
        }
        
        // 记录开始时间
        node->start_time = infra_time_monotonic();
        
        // 如果任务已取消，跳过处理
        if (node->cancelled) {
            free(node);
            continue;
        }
        
        // 处理任务
        result = process_task(&node->task);
        
        // 记录完成时间
        node->complete_time = infra_time_monotonic();
        
        // 调用回调函数
        if (node->task.callback) {
            node->task.callback(&node->task, result);
        }
        
        // 释放节点
        free(node);
    }
    
    return NULL;
}

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

infra_error_t infra_async_init(infra_async_t* async, const infra_config_t* config) {
    if (!async || !config) return INFRA_ERROR_INVALID;
    
    // 初始化成员
    memset(async, 0, sizeof(infra_async_t));
    async->stop = false;  // 初始状态为运行
    
    // 创建任务队列
    queue_init(&async->task_queue);
    
    // 启动工作线程
    infra_error_t err = infra_thread_create(&async->worker, worker_thread, async);
    if (err != INFRA_OK) {
        queue_cleanup(&async->task_queue);
        return err;
    }
    
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
}

infra_error_t infra_async_submit(infra_async_t* async, infra_async_task_t* task) {
    if (!async || !task) return INFRA_ERROR_INVALID;
    return queue_push(&async->task_queue, task);
}

infra_error_t infra_async_run(infra_async_t* async, uint32_t timeout_ms) {
    if (!async) return INFRA_ERROR_INVALID;
    
    // 等待任务完成或超时
    infra_time_t start_time = infra_time_monotonic();
    
    while (!async->stop) {
        // 检查是否超时
        infra_time_t current_time = infra_time_monotonic();
        if (timeout_ms > 0 && current_time - start_time >= timeout_ms * 1000) {  // 转换为微秒
            return INFRA_ERROR_TIMEOUT;
        }
        
        // 检查队列是否为空
        infra_platform_mutex_lock(async->task_queue.mutex);
        if (async->task_queue.size == 0) {
            // 队列为空，等待新任务或超时
            infra_platform_cond_timedwait(async->task_queue.not_empty, 
                                        async->task_queue.mutex, 
                                        100);  // 等待100ms
        }
        infra_platform_mutex_unlock(async->task_queue.mutex);
        
        // 再次检查队列，如果还是空，说明任务已完成
        infra_platform_mutex_lock(async->task_queue.mutex);
        bool is_empty = (async->task_queue.size == 0);
        infra_platform_mutex_unlock(async->task_queue.mutex);
        
        if (is_empty) {
            return INFRA_OK;
        }
    }
    
    return INFRA_OK;
}

infra_error_t infra_async_cancel(infra_async_t* async, infra_async_task_t* task) {
    if (!async || !task) return INFRA_ERROR_INVALID;
    // TODO: 实现任务取消
    return INFRA_OK;
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
    // TODO: 实现统计信息收集
    return INFRA_OK;
}
