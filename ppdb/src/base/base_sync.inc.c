/*
 * base_sync.inc.c - Synchronization Primitives Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

#define SYNC_ALIGNMENT 16

// Time functions
static uint64_t time_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

// Helper functions
static void sync_backoff(uint32_t backoff_us) {
    if (backoff_us == 0) return;
    usleep(backoff_us);  // Cosmopolitan's usleep
}

static void sync_update_stats(ppdb_base_sync_t* sync, uint64_t wait_time_us) {
    // TODO: Implement statistics collection
    (void)sync;
    (void)wait_time_us;
}

/*
 * Mutex Implementation
 */

struct ppdb_base_mutex_s {
    pthread_mutex_t mutex;
    bool initialized;
    bool recursive;
    _Atomic(uint64_t) lock_count;
    _Atomic(uint64_t) contention_count;
    _Atomic(uint64_t) total_wait_time_us;
    _Atomic(uint64_t) max_wait_time_us;
    pthread_t owner;
    char error_msg[256];
};

ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex) {
    if (!mutex) return PPDB_ERR_PARAM;

    ppdb_base_mutex_t* m = (ppdb_base_mutex_t*)ppdb_base_aligned_alloc(16, sizeof(ppdb_base_mutex_t));
    if (!m) return PPDB_ERR_MEMORY;

    memset(m, 0, sizeof(ppdb_base_mutex_t));
    
    // 初始化互斥锁属性
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    
    // 创建互斥锁
    if (pthread_mutex_init(&m->mutex, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        ppdb_base_aligned_free(m);
        return PPDB_ERR_MEMORY;
    }
    
    pthread_mutexattr_destroy(&attr);
    
    m->initialized = true;
    m->recursive = true;
    atomic_store_explicit(&m->lock_count, 0, memory_order_release);
    atomic_store_explicit(&m->contention_count, 0, memory_order_release);
    atomic_store_explicit(&m->total_wait_time_us, 0, memory_order_release);
    atomic_store_explicit(&m->max_wait_time_us, 0, memory_order_release);
    
    *mutex = m;
    return PPDB_OK;
}

void ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex) {
    if (!mutex) return;
    if (mutex->initialized) {
        pthread_mutex_destroy(&mutex->mutex);
    }
    ppdb_base_aligned_free(mutex);
}

ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_PARAM;
    if (!mutex->initialized) return PPDB_ERR_INVALID_STATE;

    uint64_t start_time = time_now_us();
    int result = pthread_mutex_lock(&mutex->mutex);
    
    if (result != 0) {
        snprintf(mutex->error_msg, sizeof(mutex->error_msg), "Failed to lock mutex: %s", strerror(result));
        return PPDB_ERR_INVALID_STATE;
    }
    
    // 更新统计信息
    uint64_t wait_time = time_now_us() - start_time;
    atomic_fetch_add_explicit(&mutex->lock_count, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&mutex->total_wait_time_us, wait_time, memory_order_relaxed);
    
    uint64_t current_max = atomic_load_explicit(&mutex->max_wait_time_us, memory_order_relaxed);
    while (wait_time > current_max) {
        if (atomic_compare_exchange_weak_explicit(&mutex->max_wait_time_us, &current_max, wait_time,
                                                memory_order_relaxed, memory_order_relaxed)) {
            break;
        }
    }
    
    if (wait_time > 1000) { // 1ms 以上视为竞争
        atomic_fetch_add_explicit(&mutex->contention_count, 1, memory_order_relaxed);
    }
    
    mutex->owner = pthread_self();
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_trylock(ppdb_base_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_PARAM;
    if (!mutex->initialized) return PPDB_ERR_INVALID_STATE;

    int result = pthread_mutex_trylock(&mutex->mutex);
    if (result == 0) {
        atomic_fetch_add_explicit(&mutex->lock_count, 1, memory_order_relaxed);
        mutex->owner = pthread_self();
        return PPDB_OK;
    } else if (result == EBUSY) {
        return PPDB_ERR_BUSY;
    } else {
        snprintf(mutex->error_msg, sizeof(mutex->error_msg), "Failed to trylock mutex: %s", strerror(result));
        return PPDB_ERR_INVALID_STATE;
    }
}

ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_PARAM;
    if (!mutex->initialized) return PPDB_ERR_INVALID_STATE;

    // 检查是否是锁的拥有者
    if (!pthread_equal(mutex->owner, pthread_self())) {
        snprintf(mutex->error_msg, sizeof(mutex->error_msg), "Attempt to unlock mutex not owned by current thread");
        return PPDB_ERR_INVALID_STATE;
    }

    int result = pthread_mutex_unlock(&mutex->mutex);
    if (result != 0) {
        snprintf(mutex->error_msg, sizeof(mutex->error_msg), "Failed to unlock mutex: %s", strerror(result));
        return PPDB_ERR_INVALID_STATE;
    }
    
    mutex->owner = 0;
    return PPDB_OK;
}

// 获取互斥锁统计信息
void ppdb_base_mutex_get_stats(ppdb_base_mutex_t* mutex, uint64_t* lock_count,
                              uint64_t* contention_count, uint64_t* total_wait_time_us,
                              uint64_t* max_wait_time_us) {
    if (!mutex) return;
    
    if (lock_count)
        *lock_count = atomic_load_explicit(&mutex->lock_count, memory_order_relaxed);
    if (contention_count)
        *contention_count = atomic_load_explicit(&mutex->contention_count, memory_order_relaxed);
    if (total_wait_time_us)
        *total_wait_time_us = atomic_load_explicit(&mutex->total_wait_time_us, memory_order_relaxed);
    if (max_wait_time_us)
        *max_wait_time_us = atomic_load_explicit(&mutex->max_wait_time_us, memory_order_relaxed);
}

// 获取互斥锁错误信息
const char* ppdb_base_mutex_get_error(ppdb_base_mutex_t* mutex) {
    if (!mutex) return "Invalid mutex";
    return mutex->error_msg;
}

/*
 * Sync Object Implementation
 */

ppdb_error_t ppdb_base_sync_create(ppdb_base_sync_t** sync, const ppdb_base_sync_config_t* config) {
    if (!sync || !config) return PPDB_ERR_PARAM;
    if (*sync) return PPDB_ERR_EXISTS;

    ppdb_base_sync_t* s = (ppdb_base_sync_t*)ppdb_base_aligned_alloc(SYNC_ALIGNMENT, sizeof(ppdb_base_sync_t));
    if (!s) return PPDB_ERR_MEMORY;

    memset(s, 0, sizeof(ppdb_base_sync_t));
    s->config = *config;

    if (config->thread_safe) {
        ppdb_error_t err = ppdb_base_mutex_create(&s->mutex);
        if (err != PPDB_OK) {
            ppdb_base_aligned_free(s);
            return err;
        }
    }

    *sync = s;
    return PPDB_OK;
}

void ppdb_base_sync_destroy(ppdb_base_sync_t* sync) {
    if (!sync) return;

    if (sync->mutex) {
        ppdb_base_mutex_destroy(sync->mutex);
    }

    ppdb_base_aligned_free(sync);
}

// Lock operations
ppdb_error_t ppdb_base_sync_lock(ppdb_base_sync_t* sync) {
    if (!sync) return PPDB_ERR_PARAM;

    uint64_t start_time = time_now_us();
    uint32_t backoff = 0;

    while (true) {
        // Try to acquire lock
        if (!sync->writer && __sync_bool_compare_and_swap(&sync->writer, false, true)) {
            sync_update_stats(sync, time_now_us() - start_time);
            return PPDB_OK;
        }

        // Backoff if configured
        if (sync->config.backoff_us > 0) {
            backoff = backoff ? backoff * 2 : sync->config.backoff_us;
            sync_backoff(backoff);
        }
    }
}

ppdb_error_t ppdb_base_sync_unlock(ppdb_base_sync_t* sync) {
    if (!sync) return PPDB_ERR_PARAM;
    if (!sync->writer) return PPDB_ERR_INVALID_STATE;

    sync->writer = false;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_sync_try_lock(ppdb_base_sync_t* sync) {
    if (!sync) return PPDB_ERR_PARAM;

    if (!sync->writer && __sync_bool_compare_and_swap(&sync->writer, false, true)) {
        return PPDB_OK;
    }

    return PPDB_ERR_BUSY;
}

/*
 * Read-Write Lock Implementation
 */

ppdb_error_t ppdb_base_sync_read_lock(ppdb_base_sync_t* sync) {
    if (!sync) return PPDB_ERR_PARAM;

    uint64_t start_time = time_now_us();
    uint32_t backoff = 0;

    while (true) {
        // Try to acquire read lock
        if (!sync->writer) {
            __sync_fetch_and_add(&sync->readers, 1);
            if (!sync->writer) {
                sync_update_stats(sync, time_now_us() - start_time);
                return PPDB_OK;
            }
            __sync_fetch_and_sub(&sync->readers, 1);
        }

        // Backoff if configured
        if (sync->config.backoff_us > 0) {
            backoff = backoff ? backoff * 2 : sync->config.backoff_us;
            sync_backoff(backoff);
        }
    }
}

ppdb_error_t ppdb_base_sync_read_unlock(ppdb_base_sync_t* sync) {
    if (!sync) return PPDB_ERR_PARAM;
    if (sync->readers == 0) return PPDB_ERR_INVALID_STATE;

    __sync_fetch_and_sub(&sync->readers, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_sync_try_read_lock(ppdb_base_sync_t* sync) {
    if (!sync) return PPDB_ERR_PARAM;

    if (!sync->writer) {
        __sync_fetch_and_add(&sync->readers, 1);
        if (!sync->writer) {
            return PPDB_OK;
        }
        __sync_fetch_and_sub(&sync->readers, 1);
    }

    return PPDB_ERR_BUSY;
}

ppdb_error_t ppdb_base_sync_write_lock(ppdb_base_sync_t* sync) {
    if (!sync) return PPDB_ERR_PARAM;

    uint64_t start_time = time_now_us();
    uint32_t backoff = 0;

    while (true) {
        // Try to acquire write lock
        if (!sync->writer && sync->readers == 0 &&
            __sync_bool_compare_and_swap(&sync->writer, false, true)) {
            sync_update_stats(sync, time_now_us() - start_time);
            return PPDB_OK;
        }

        // Backoff if configured
        if (sync->config.backoff_us > 0) {
            backoff = backoff ? backoff * 2 : sync->config.backoff_us;
            sync_backoff(backoff);
        }
    }
}

ppdb_error_t ppdb_base_sync_write_unlock(ppdb_base_sync_t* sync) {
    if (!sync) return PPDB_ERR_PARAM;
    if (!sync->writer) return PPDB_ERR_INVALID_STATE;

    sync->writer = false;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_sync_try_write_lock(ppdb_base_sync_t* sync) {
    if (!sync) return PPDB_ERR_PARAM;

    if (!sync->writer && sync->readers == 0 &&
        __sync_bool_compare_and_swap(&sync->writer, false, true)) {
        return PPDB_OK;
    }

    return PPDB_ERR_BUSY;
}

/*
 * Spinlock Implementation
 */

struct ppdb_base_spinlock_s {
    atomic_flag flag;
    bool initialized;
    _Atomic(uint32_t) spin_count;
    _Atomic(uint64_t) lock_count;
    _Atomic(uint64_t) contention_count;
    _Atomic(uint64_t) total_wait_time_us;
    _Atomic(uint64_t) max_wait_time_us;
    pthread_t owner;
    char error_msg[256];
};

ppdb_error_t ppdb_base_spinlock_create(ppdb_base_spinlock_t** spinlock) {
    if (!spinlock) return PPDB_ERR_PARAM;

    ppdb_base_spinlock_t* s = (ppdb_base_spinlock_t*)ppdb_base_aligned_alloc(16, sizeof(ppdb_base_spinlock_t));
    if (!s) return PPDB_ERR_MEMORY;

    memset(s, 0, sizeof(ppdb_base_spinlock_t));
    atomic_flag_clear(&s->flag);
    s->initialized = true;
    atomic_store_explicit(&s->spin_count, 1000, memory_order_release);
    atomic_store_explicit(&s->lock_count, 0, memory_order_release);
    atomic_store_explicit(&s->contention_count, 0, memory_order_release);
    atomic_store_explicit(&s->total_wait_time_us, 0, memory_order_release);
    atomic_store_explicit(&s->max_wait_time_us, 0, memory_order_release);
    
    *spinlock = s;
    return PPDB_OK;
}

void ppdb_base_spinlock_destroy(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock) return;
    ppdb_base_aligned_free(spinlock);
}

ppdb_error_t ppdb_base_spinlock_lock(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock) return PPDB_ERR_PARAM;
    if (!spinlock->initialized) return PPDB_ERR_INVALID_STATE;

    uint64_t start_time = time_now_us();
    uint32_t spins = 0;
    uint32_t max_spins = atomic_load_explicit(&spinlock->spin_count, memory_order_relaxed);
    
    // 快速路径：尝试一次获取锁
    if (!atomic_flag_test_and_set(&spinlock->flag)) {
        spinlock->owner = pthread_self();
        atomic_fetch_add_explicit(&spinlock->lock_count, 1, memory_order_relaxed);
        return PPDB_OK;
    }
    
    // 慢速路径：自旋等待
    while (true) {
        spins++;
        
        // 自适应自旋
        for (uint32_t i = 0; i < (spins < 32u ? spins : 32u); i++) {
            __builtin_ia32_pause();
        }
        
        // 尝试获取锁
        if (!atomic_flag_test_and_set(&spinlock->flag)) {
            uint64_t wait_time = time_now_us() - start_time;
            
            // 更新统计信息
            atomic_fetch_add_explicit(&spinlock->lock_count, 1, memory_order_relaxed);
            atomic_fetch_add_explicit(&spinlock->total_wait_time_us, wait_time, memory_order_relaxed);
            
            uint64_t current_max = atomic_load_explicit(&spinlock->max_wait_time_us, memory_order_relaxed);
            while (wait_time > current_max) {
                if (atomic_compare_exchange_weak_explicit(&spinlock->max_wait_time_us, &current_max, wait_time,
                                                        memory_order_relaxed, memory_order_relaxed)) {
                    break;
                }
            }
            
            if (spins > 1) {
                atomic_fetch_add_explicit(&spinlock->contention_count, 1, memory_order_relaxed);
            }
            
            spinlock->owner = pthread_self();
            return PPDB_OK;
        }
        
        // 如果自旋次数过多，让出 CPU
        if (spins >= max_spins) {
            sched_yield();
            spins = 0;
        }
    }
}

ppdb_error_t ppdb_base_spinlock_trylock(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock) return PPDB_ERR_PARAM;
    if (!spinlock->initialized) return PPDB_ERR_INVALID_STATE;

    if (!atomic_flag_test_and_set(&spinlock->flag)) {
        atomic_fetch_add_explicit(&spinlock->lock_count, 1, memory_order_relaxed);
        spinlock->owner = pthread_self();
        return PPDB_OK;
    }
    
    return PPDB_ERR_BUSY;
}

ppdb_error_t ppdb_base_spinlock_unlock(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock) return PPDB_ERR_PARAM;
    if (!spinlock->initialized) return PPDB_ERR_INVALID_STATE;

    // 检查是否是锁的拥有者
    if (!pthread_equal(spinlock->owner, pthread_self())) {
        snprintf(spinlock->error_msg, sizeof(spinlock->error_msg), 
                "Attempt to unlock spinlock not owned by current thread");
        return PPDB_ERR_INVALID_STATE;
    }

    spinlock->owner = 0;
    atomic_flag_clear(&spinlock->flag);
    return PPDB_OK;
}

// 设置自旋次数
void ppdb_base_spinlock_set_spin_count(ppdb_base_spinlock_t* spinlock, uint32_t count) {
    if (!spinlock) return;
    atomic_store_explicit(&spinlock->spin_count, count, memory_order_release);
}

// 获取自旋锁统计信息
void ppdb_base_spinlock_get_stats(ppdb_base_spinlock_t* spinlock, uint64_t* lock_count,
                                 uint64_t* contention_count, uint64_t* total_wait_time_us,
                                 uint64_t* max_wait_time_us) {
    if (!spinlock) return;
    
    if (lock_count)
        *lock_count = atomic_load_explicit(&spinlock->lock_count, memory_order_relaxed);
    if (contention_count)
        *contention_count = atomic_load_explicit(&spinlock->contention_count, memory_order_relaxed);
    if (total_wait_time_us)
        *total_wait_time_us = atomic_load_explicit(&spinlock->total_wait_time_us, memory_order_relaxed);
    if (max_wait_time_us)
        *max_wait_time_us = atomic_load_explicit(&spinlock->max_wait_time_us, memory_order_relaxed);
}

// 获取自旋锁错误信息
const char* ppdb_base_spinlock_get_error(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock) return "Invalid spinlock";
    return spinlock->error_msg;
}

/*
 * Thread Implementation
 */

struct ppdb_base_thread_s {
    pthread_t thread;
    ppdb_base_thread_func_t func;
    void* arg;
    bool initialized;
    bool joined;
    bool detached;
    _Atomic(int) state;  // 0: created, 1: running, 2: finished, -1: error
    _Atomic(uint64_t) cpu_time_us;
    _Atomic(uint64_t) wall_time_us;
    void* result;
    char error_msg[256];
};

static void* thread_wrapper(void* arg) {
    ppdb_base_thread_t* t = (ppdb_base_thread_t*)arg;
    void* result = NULL;
    uint64_t start_time = time_now_us();
    
    // 设置线程状态为运行中
    atomic_store_explicit(&t->state, 1, memory_order_release);
    
    // 执行用户函数
    result = t->func(t->arg);
    
    // 更新统计信息
    uint64_t end_time = time_now_us();
    atomic_store_explicit(&t->wall_time_us, end_time - start_time, memory_order_release);
    
    // 保存结果
    t->result = result;
    
    // 设置线程状态为已完成
    atomic_store_explicit(&t->state, 2, memory_order_release);
    
    return result;
}

ppdb_error_t ppdb_base_thread_create(ppdb_base_thread_t** thread, ppdb_base_thread_func_t func, void* arg) {
    if (!thread || !func) return PPDB_ERR_PARAM;

    // 分配线程对象
    ppdb_base_thread_t* t = (ppdb_base_thread_t*)ppdb_base_aligned_alloc(16, sizeof(ppdb_base_thread_t));
    if (!t) return PPDB_ERR_MEMORY;

    // 初始化线程对象
    memset(t, 0, sizeof(ppdb_base_thread_t));
    t->func = func;
    t->arg = arg;
    t->initialized = true;
    t->joined = false;
    t->detached = false;
    atomic_store_explicit(&t->state, 0, memory_order_release);
    atomic_store_explicit(&t->cpu_time_us, 0, memory_order_release);
    atomic_store_explicit(&t->wall_time_us, 0, memory_order_release);

    // 设置线程属性
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    // 创建线程
    if (pthread_create(&t->thread, &attr, thread_wrapper, t) != 0) {
        snprintf(t->error_msg, sizeof(t->error_msg), "Failed to create thread");
        atomic_store_explicit(&t->state, -1, memory_order_release);
        pthread_attr_destroy(&attr);
        ppdb_base_aligned_free(t);
        return PPDB_BASE_ERR_THREAD;
    }

    pthread_attr_destroy(&attr);
    *thread = t;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_join(ppdb_base_thread_t* thread, void** retval) {
    if (!thread) return PPDB_ERR_PARAM;
    if (!thread->initialized) return PPDB_ERR_INVALID_STATE;
    if (thread->joined) return PPDB_ERR_INVALID_STATE;
    if (thread->detached) return PPDB_ERR_INVALID_STATE;

    // 等待线程完成
    void* thread_retval;
    if (pthread_join(thread->thread, &thread_retval) != 0) {
        snprintf(thread->error_msg, sizeof(thread->error_msg), "Failed to join thread");
        atomic_store_explicit(&thread->state, -1, memory_order_release);
        return PPDB_BASE_ERR_THREAD;
    }

    // 保存返回值
    if (retval) {
        *retval = thread_retval;
    }

    thread->joined = true;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_detach(ppdb_base_thread_t* thread) {
    if (!thread) return PPDB_ERR_PARAM;
    if (!thread->initialized) return PPDB_ERR_INVALID_STATE;
    if (thread->joined) return PPDB_ERR_INVALID_STATE;
    if (thread->detached) return PPDB_ERR_INVALID_STATE;

    if (pthread_detach(thread->thread) != 0) {
        snprintf(thread->error_msg, sizeof(thread->error_msg), "Failed to detach thread");
        atomic_store_explicit(&thread->state, -1, memory_order_release);
        return PPDB_BASE_ERR_THREAD;
    }

    thread->detached = true;
    return PPDB_OK;
}

void ppdb_base_thread_destroy(ppdb_base_thread_t* thread) {
    if (!thread) return;
    
    // 如果线程未完成且未分离，先分离线程
    if (thread->initialized && !thread->joined && !thread->detached) {
        pthread_detach(thread->thread);
    }
    
    ppdb_base_aligned_free(thread);
}

// 获取线程状态
int ppdb_base_thread_get_state(ppdb_base_thread_t* thread) {
    if (!thread) return -1;
    return atomic_load_explicit(&thread->state, memory_order_acquire);
}

// 获取线程运行时间
uint64_t ppdb_base_thread_get_wall_time(ppdb_base_thread_t* thread) {
    if (!thread) return 0;
    return atomic_load_explicit(&thread->wall_time_us, memory_order_acquire);
}

// 获取线程错误信息
const char* ppdb_base_thread_get_error(ppdb_base_thread_t* thread) {
    if (!thread) return "Invalid thread";
    return thread->error_msg;
}

// Sync initialization
ppdb_error_t ppdb_base_sync_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_PARAM;

    // Create global mutex
    ppdb_error_t err = ppdb_base_mutex_create(&base->global_mutex);
    if (err != PPDB_OK) return err;

    // Initialize sync configuration
    base->sync_config.thread_safe = true;
    base->sync_config.spin_count = 1000;
    base->sync_config.backoff_us = 1;

    return PPDB_OK;
}

void ppdb_base_sync_cleanup(ppdb_base_t* base) {
    if (!base) return;

    if (base->global_mutex) {
        ppdb_base_mutex_destroy(base->global_mutex);
        base->global_mutex = NULL;
    }
}