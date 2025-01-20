/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_sync.h - Synchronization Primitives Interface
 */

#ifndef INFRA_SYNC_H
#define INFRA_SYNC_H

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

typedef void* infra_mutex_t;
typedef void* infra_cond_t;
typedef void* infra_rwlock_t;
typedef void* infra_thread_t;

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

infra_error_t infra_mutex_create(infra_mutex_t* mutex);
void infra_mutex_destroy(infra_mutex_t mutex);
infra_error_t infra_mutex_lock(infra_mutex_t mutex);
infra_error_t infra_mutex_trylock(infra_mutex_t mutex);
infra_error_t infra_mutex_unlock(infra_mutex_t mutex);

//-----------------------------------------------------------------------------
// Condition Variable Operations
//-----------------------------------------------------------------------------

infra_error_t infra_cond_init(infra_cond_t* cond);
void infra_cond_destroy(infra_cond_t cond);
infra_error_t infra_cond_wait(infra_cond_t cond, infra_mutex_t mutex);
infra_error_t infra_cond_timedwait(infra_cond_t cond, infra_mutex_t mutex, uint32_t timeout_ms);
infra_error_t infra_cond_signal(infra_cond_t cond);
infra_error_t infra_cond_broadcast(infra_cond_t cond);

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

infra_error_t infra_rwlock_init(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_destroy(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_rdlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_tryrdlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_wrlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_trywrlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_unlock(infra_rwlock_t rwlock);

//-----------------------------------------------------------------------------
// Thread Operations
//-----------------------------------------------------------------------------

typedef void* (*infra_thread_func_t)(void*);

infra_error_t infra_thread_create(infra_thread_t* thread, infra_thread_func_t func, void* arg);
infra_error_t infra_thread_join(infra_thread_t thread);
infra_error_t infra_thread_detach(infra_thread_t thread);

//-----------------------------------------------------------------------------
// Spinlock Operations
//-----------------------------------------------------------------------------

typedef struct {
    volatile int32_t lock;
} infra_spinlock_t;

void infra_spinlock_init(infra_spinlock_t* spinlock);
void infra_spinlock_destroy(infra_spinlock_t* spinlock);
void infra_spinlock_lock(infra_spinlock_t* spinlock);
bool infra_spinlock_trylock(infra_spinlock_t* spinlock);
void infra_spinlock_unlock(infra_spinlock_t* spinlock);

//-----------------------------------------------------------------------------
// Semaphore Operations
//-----------------------------------------------------------------------------

typedef struct {
    volatile int32_t value;
    infra_mutex_t mutex;
    infra_cond_t cond;
} infra_sem_t;

infra_error_t infra_sem_init(infra_sem_t* sem, uint32_t value);
void infra_sem_destroy(infra_sem_t* sem);
infra_error_t infra_sem_wait(infra_sem_t* sem);
infra_error_t infra_sem_trywait(infra_sem_t* sem);
infra_error_t infra_sem_timedwait(infra_sem_t* sem, uint32_t timeout_ms);
infra_error_t infra_sem_post(infra_sem_t* sem);
infra_error_t infra_sem_getvalue(infra_sem_t* sem, int* value);

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

infra_error_t infra_yield(void);
infra_error_t infra_sleep(uint32_t milliseconds);

//-----------------------------------------------------------------------------
// Thread Pool
//-----------------------------------------------------------------------------

// 线程池任务结构
typedef struct infra_task {
    infra_thread_func_t func;
    void* arg;
    struct infra_task* next;
} infra_task_t;

// 线程池句柄
typedef struct infra_thread_pool {
    infra_thread_t* threads;
    size_t thread_count;
    size_t min_threads;
    size_t max_threads;
    size_t queue_size;
    uint32_t idle_timeout;
    
    infra_mutex_t mutex;
    infra_cond_t not_empty;
    infra_cond_t not_full;
    
    infra_task_t* task_head;
    infra_task_t* task_tail;
    size_t task_count;
    size_t active_count;
    
    bool running;
    bool shutting_down;
} infra_thread_pool_t;

// 线程池配置
typedef struct {
    size_t min_threads;     // 最小线程数
    size_t max_threads;     // 最大线程数
    size_t queue_size;      // 任务队列大小
    uint32_t idle_timeout;  // 空闲线程超时时间(ms)
} infra_thread_pool_config_t;

// 线程池操作
infra_error_t infra_thread_pool_create(const infra_thread_pool_config_t* config, infra_thread_pool_t** pool);
infra_error_t infra_thread_pool_destroy(infra_thread_pool_t* pool);
infra_error_t infra_thread_pool_submit(infra_thread_pool_t* pool, infra_thread_func_t func, void* arg);
infra_error_t infra_thread_pool_get_stats(infra_thread_pool_t* pool, size_t* active_threads, size_t* queued_tasks);

#endif // INFRA_SYNC_H 