/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_sync.c - Synchronization Primitives Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_sync.h"

// 工作线程函数的前向声明
static void* worker_thread(void* arg);

// 工作线程函数实现
static void* worker_thread(void* arg) {
    infra_thread_pool_t* pool = (infra_thread_pool_t*)arg;
    infra_task_t* task = NULL;
    
    while (pool->running) {
        // 获取任务
        infra_mutex_lock(pool->mutex);
        
        while (pool->task_head == NULL && !pool->shutting_down) {
            // 等待任务或关闭信号
            infra_error_t err = infra_cond_timedwait(pool->not_empty, pool->mutex, pool->idle_timeout);
            if (err == INFRA_ERROR_TIMEOUT) {
                // 超时，如果超过最小线程数，则退出
                if (pool->thread_count > pool->min_threads) {
                    pool->thread_count--;
                    infra_mutex_unlock(pool->mutex);
                    return NULL;
                }
            }
        }
        
        if (pool->shutting_down && pool->task_head == NULL) {
            pool->thread_count--;
            infra_mutex_unlock(pool->mutex);
            return NULL;
        }
        
        // 获取任务
        task = pool->task_head;
        if (task != NULL) {
            pool->task_head = task->next;
            if (pool->task_head == NULL) {
                pool->task_tail = NULL;
            }
            pool->task_count--;
            pool->active_count++;
            infra_mutex_unlock(pool->mutex);
            
            // 执行任务
            task->func(task->arg);
            infra_free(task);
            
            infra_mutex_lock(pool->mutex);
            pool->active_count--;
            if (pool->task_count < pool->queue_size) {
                infra_cond_signal(pool->not_full);
            }
            infra_mutex_unlock(pool->mutex);
        } else {
            infra_mutex_unlock(pool->mutex);
        }
    }
    
    return NULL;
}

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

infra_error_t infra_mutex_create(infra_mutex_t* mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_create((void**)mutex);
}

void infra_mutex_destroy(infra_mutex_t mutex) {
    if (mutex) {
        infra_platform_mutex_destroy(mutex);
    }
}

infra_error_t infra_mutex_lock(infra_mutex_t mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_lock(mutex);
}

infra_error_t infra_mutex_trylock(infra_mutex_t mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_trylock(mutex);
}

infra_error_t infra_mutex_unlock(infra_mutex_t mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_unlock(mutex);
}

//-----------------------------------------------------------------------------
// Condition Variable Operations
//-----------------------------------------------------------------------------

infra_error_t infra_cond_init(infra_cond_t* cond) {
    if (!cond) return INFRA_ERROR_INVALID;
    return infra_platform_cond_create((void**)cond);
}

void infra_cond_destroy(infra_cond_t cond) {
    if (cond) {
        infra_platform_cond_destroy(cond);
    }
}

infra_error_t infra_cond_wait(infra_cond_t cond, infra_mutex_t mutex) {
    if (!cond || !mutex) return INFRA_ERROR_INVALID;
    return infra_platform_cond_wait(cond, mutex);
}

infra_error_t infra_cond_timedwait(infra_cond_t cond, infra_mutex_t mutex, uint32_t timeout_ms) {
    if (!cond || !mutex) return INFRA_ERROR_INVALID;
    return infra_platform_cond_timedwait(cond, mutex, timeout_ms);
}

infra_error_t infra_cond_signal(infra_cond_t cond) {
    if (!cond) return INFRA_ERROR_INVALID;
    return infra_platform_cond_signal(cond);
}

infra_error_t infra_cond_broadcast(infra_cond_t cond) {
    if (!cond) return INFRA_ERROR_INVALID;
    return infra_platform_cond_broadcast(cond);
}

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

infra_error_t infra_rwlock_init(infra_rwlock_t* rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_create((void**)rwlock);
}

infra_error_t infra_rwlock_destroy(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    infra_platform_rwlock_destroy(rwlock);
    return INFRA_OK;
}

infra_error_t infra_rwlock_rdlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_rdlock(rwlock);
}

infra_error_t infra_rwlock_tryrdlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_tryrdlock(rwlock);
}

infra_error_t infra_rwlock_wrlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_wrlock(rwlock);
}

infra_error_t infra_rwlock_trywrlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_trywrlock(rwlock);
}

infra_error_t infra_rwlock_unlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_unlock(rwlock);
}

//-----------------------------------------------------------------------------
// Thread Operations
//-----------------------------------------------------------------------------

infra_error_t infra_thread_create(infra_thread_t* thread, infra_thread_func_t func, void* arg) {
    if (!thread || !func) return INFRA_ERROR_INVALID;
    return infra_platform_thread_create((void**)thread, func, arg);
}

infra_error_t infra_thread_join(infra_thread_t thread) {
    if (!thread) return INFRA_ERROR_INVALID;
    return infra_platform_thread_join(thread);
}

infra_error_t infra_thread_detach(infra_thread_t thread) {
    if (!thread) return INFRA_ERROR_INVALID;
    return infra_platform_thread_detach(thread);
}

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

infra_error_t infra_yield(void) {
    return infra_platform_yield();
}

infra_error_t infra_sleep(uint32_t milliseconds) {
    return infra_platform_sleep(milliseconds);
}

// 线程池相关实现
infra_error_t infra_thread_pool_create(const infra_thread_pool_config_t* config, infra_thread_pool_t** pool) {
    if (!config || !pool) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 验证配置参数
    if (config->min_threads == 0 || config->max_threads == 0 || 
        config->min_threads > config->max_threads || 
        config->queue_size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 分配线程池结构
    infra_thread_pool_t* p = (infra_thread_pool_t*)infra_malloc(sizeof(infra_thread_pool_t));
    if (!p) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化配置
    p->min_threads = config->min_threads;
    p->max_threads = config->max_threads;
    p->queue_size = config->queue_size;
    p->idle_timeout = config->idle_timeout;

    // 初始化同步原语
    infra_error_t err = INFRA_OK;
    err = infra_mutex_create(&p->mutex);
    if (err != INFRA_OK) {
        infra_free(p);
        return err;
    }

    err = infra_cond_init(&p->not_empty);
    if (err != INFRA_OK) {
        infra_mutex_destroy(p->mutex);
        infra_free(p);
        return err;
    }

    err = infra_cond_init(&p->not_full);
    if (err != INFRA_OK) {
        infra_mutex_destroy(p->mutex);
        infra_cond_destroy(p->not_empty);
        infra_free(p);
        return err;
    }

    // 分配线程数组
    p->threads = (infra_thread_t*)infra_malloc(sizeof(infra_thread_t) * p->max_threads);
    if (!p->threads) {
        infra_mutex_destroy(p->mutex);
        infra_cond_destroy(p->not_empty);
        infra_cond_destroy(p->not_full);
        infra_free(p);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化状态
    p->thread_count = 0;
    p->active_count = 0;
    p->task_head = NULL;
    p->task_tail = NULL;
    p->task_count = 0;
    p->running = true;
    p->shutting_down = false;

    // 创建初始线程
    for (size_t i = 0; i < p->min_threads; i++) {
        err = infra_thread_create(&p->threads[i], worker_thread, p);
        if (err != INFRA_OK) {
            // 清理已创建的线程
            p->shutting_down = true;
            infra_cond_broadcast(p->not_empty);
            for (size_t j = 0; j < i; j++) {
                infra_thread_join(p->threads[j]);
            }
            infra_free(p->threads);
            infra_mutex_destroy(p->mutex);
            infra_cond_destroy(p->not_empty);
            infra_cond_destroy(p->not_full);
            infra_free(p);
            return err;
        }
        p->thread_count++;
    }

    *pool = p;
    return INFRA_OK;
}

infra_error_t infra_thread_pool_destroy(infra_thread_pool_t* pool) {
    if (!pool) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 标记关闭
    infra_mutex_lock(pool->mutex);
    pool->shutting_down = true;
    infra_cond_broadcast(pool->not_empty);
    infra_mutex_unlock(pool->mutex);

    // 等待所有线程结束
    for (size_t i = 0; i < pool->thread_count; i++) {
        infra_thread_join(pool->threads[i]);
    }

    // 清理任务队列
    infra_task_t* task = pool->task_head;
    while (task) {
        infra_task_t* next = task->next;
        infra_free(task);
        task = next;
    }

    // 清理资源
    infra_free(pool->threads);
    infra_mutex_destroy(pool->mutex);
    infra_cond_destroy(pool->not_empty);
    infra_cond_destroy(pool->not_full);
    infra_free(pool);

    return INFRA_OK;
}

infra_error_t infra_thread_pool_submit(infra_thread_pool_t* pool, infra_thread_func_t func, void* arg) {
    if (!pool || !func) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建任务
    infra_task_t* task = (infra_task_t*)infra_malloc(sizeof(infra_task_t));
    if (!task) {
        return INFRA_ERROR_NO_MEMORY;
    }
    task->func = func;
    task->arg = arg;
    task->next = NULL;

    // 加入任务队列
    infra_mutex_lock(pool->mutex);

    // 检查队列是否已满
    while (pool->task_count >= pool->queue_size && !pool->shutting_down) {
        infra_cond_wait(pool->not_full, pool->mutex);
    }

    if (pool->shutting_down) {
        infra_mutex_unlock(pool->mutex);
        infra_free(task);
        return INFRA_ERROR_NOT_READY;
    }

    // 添加任务
    if (pool->task_tail) {
        pool->task_tail->next = task;
    } else {
        pool->task_head = task;
    }
    pool->task_tail = task;
    pool->task_count++;

    // 如果有空闲线程，唤醒一个
    infra_cond_signal(pool->not_empty);

    // 如果任务较多且有空余线程槽，创建新线程
    if (pool->task_count > pool->thread_count && 
        pool->thread_count < pool->max_threads) {
        infra_thread_t thread;
        if (infra_thread_create(&thread, worker_thread, pool) == INFRA_OK) {
            pool->threads[pool->thread_count++] = thread;
        }
    }

    infra_mutex_unlock(pool->mutex);
    return INFRA_OK;
}

infra_error_t infra_thread_pool_get_stats(infra_thread_pool_t* pool, size_t* active_threads, size_t* queued_tasks) {
    if (!pool || !active_threads || !queued_tasks) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mutex_lock(pool->mutex);
    *active_threads = pool->active_count;
    *queued_tasks = pool->task_count;
    infra_mutex_unlock(pool->mutex);

    return INFRA_OK;
}

