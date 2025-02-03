// #include "internal/infra/infra_thread.h"
// #include <stdlib.h>
// #include <string.h>

// //-----------------------------------------------------------------------------
// // Thread Operations
// //-----------------------------------------------------------------------------

// infra_error_t infra_thread_create(infra_thread_t* thread, 
//                                 infra_thread_func_t fn, 
//                                 void* arg) {
//     return infra_thread_create(thread, fn, arg);
// }

// infra_error_t infra_thread_join(infra_thread_t thread) {
//     return infra_thread_join(thread);
// }

// infra_error_t infra_thread_detach(infra_thread_t thread) {
//     return infra_thread_detach(thread);
// }

// infra_thread_t infra_thread_self(void) {
//     return infra_thread_self();
// }

// //-----------------------------------------------------------------------------
// // Thread Pool Implementation
// //-----------------------------------------------------------------------------

// // 线程池内部结构
// struct infra_thread_pool {
//     infra_thread_t* threads;
//     infra_thread_func_t* tasks;
//     void** args;
//     int head;
//     int tail;
//     int count;
//     int size;
//     int num_threads;
//     bool shutdown;
//     infra_thread_pool_config_t config;
//     infra_mutex_t mutex;
//     infra_cond_t cond;
//     infra_thread_pool_stats_t stats;
// };

// // 工作线程函数
// static void* worker_thread(void* arg) {
//     infra_thread_pool_t* pool = arg;
    
//     while (1) {
//         infra_thread_func_t task = NULL;
//         void* task_arg = NULL;
        
//         infra_mutex_lock(pool->mutex);
        
//         while (pool->count == 0 && !pool->shutdown) {
//             pool->stats.idle_threads++;
//             infra_cond_wait(pool->cond, pool->mutex);
//             pool->stats.idle_threads--;
//         }
        
//         if (pool->shutdown) {
//             infra_mutex_unlock(pool->mutex);
//             break;
//         }
        
//         // 获取任务
//         task = pool->tasks[pool->head];
//         task_arg = pool->args[pool->head];
//         pool->head = (pool->head + 1) % pool->size;
//         pool->count--;
//         pool->stats.active_threads++;
//         pool->stats.queued_tasks--;
        
//         infra_mutex_unlock(pool->mutex);
        
//         // 执行任务
//         if (task) {
//             task(task_arg);
//             infra_mutex_lock(pool->mutex);
//             pool->stats.completed_tasks++;
//             pool->stats.active_threads--;
//             infra_mutex_unlock(pool->mutex);
//         } else {
//             infra_mutex_lock(pool->mutex);
//             pool->stats.failed_tasks++;
//             pool->stats.active_threads--;
//             infra_mutex_unlock(pool->mutex);
//         }
//     }
    
//     return NULL;
// }

// infra_error_t infra_thread_pool_create(const infra_thread_pool_config_t* config,
//                                      infra_thread_pool_t** pool) {
//     if (!config || !pool) return INFRA_ERROR_INVALID_PARAM;
    
//     *pool = malloc(sizeof(infra_thread_pool_t));
//     if (!*pool) return INFRA_ERROR_NO_MEMORY;
    
//     infra_thread_pool_t* p = *pool;
//     memset(p, 0, sizeof(*p));
    
//     // 初始化互斥锁和条件变量
//     if (infra_mutex_create(&p->mutex) != INFRA_OK ||
//         infra_cond_init(&p->cond) != INFRA_OK) {
//         infra_thread_pool_destroy(p);
//         return INFRA_ERROR_INIT_FAILED;
//     }
    
//     // 分配数组
//     p->size = config->queue_size;
//     p->threads = malloc(config->max_threads * sizeof(infra_thread_t));
//     p->tasks = malloc(config->queue_size * sizeof(infra_thread_func_t));
//     p->args = malloc(config->queue_size * sizeof(void*));
    
//     if (!p->threads || !p->tasks || !p->args) {
//         infra_thread_pool_destroy(p);
//         return INFRA_ERROR_NO_MEMORY;
//     }
    
//     // 保存配置
//     p->config = *config;
    
//     // 创建初始线程
//     for (int i = 0; i < config->min_threads; i++) {
//         if (infra_thread_create(&p->threads[i], worker_thread, p) != INFRA_OK) {
//             infra_thread_pool_destroy(p);
//             return INFRA_ERROR_THREAD_CREATE;
//         }
//         p->num_threads++;
//     }
    
//     return INFRA_OK;
// }

// void infra_thread_pool_destroy(infra_thread_pool_t* pool) {
//     if (!pool) return;
    
//     // 标记关闭
//     infra_mutex_lock(pool->mutex);
//     pool->shutdown = true;
//     infra_cond_broadcast(pool->cond);
//     infra_mutex_unlock(pool->mutex);
    
//     // 等待所有线程结束
//     for (int i = 0; i < pool->num_threads; i++) {
//         infra_thread_join(pool->threads[i]);
//     }
    
//     // 清理资源
//     infra_mutex_destroy(pool->mutex);
//     infra_cond_destroy(pool->cond);
//     free(pool->threads);
//     free(pool->tasks);
//     free(pool->args);
//     free(pool);
// }

// infra_error_t infra_thread_pool_submit(infra_thread_pool_t* pool,
//                                      infra_thread_func_t fn,
//                                      void* arg) {
//     if (!pool || !fn) return INFRA_ERROR_INVALID_PARAM;
    
//     infra_mutex_lock(pool->mutex);
    
//     // 检查队列是否已满
//     if (pool->count == pool->size) {
//         infra_mutex_unlock(pool->mutex);
//         return INFRA_ERROR_NO_SPACE;
//     }
    
//     // 添加任务
//     pool->tasks[pool->tail] = fn;
//     pool->args[pool->tail] = arg;
//     pool->tail = (pool->tail + 1) % pool->size;
//     pool->count++;
//     pool->stats.queued_tasks++;
    
//     // 唤醒一个工作线程
//     infra_cond_signal(pool->cond);
    
//     infra_mutex_unlock(pool->mutex);
//     return INFRA_OK;
// }

// infra_error_t infra_thread_pool_get_stats(infra_thread_pool_t* pool,
//                                         infra_thread_pool_stats_t* stats) {
//     if (!pool || !stats) return INFRA_ERROR_INVALID_PARAM;
    
//     infra_mutex_lock(pool->mutex);
//     *stats = pool->stats;
//     infra_mutex_unlock(pool->mutex);
    
//     return INFRA_OK;
// }

// //-----------------------------------------------------------------------------
// // Utility Functions
// //-----------------------------------------------------------------------------

// infra_error_t infra_yield(void) {
//     return infra_yield();
// }

// infra_error_t infra_sleep_ms(uint32_t ms) {
//     return infra_sleep_ms(ms);
// }
