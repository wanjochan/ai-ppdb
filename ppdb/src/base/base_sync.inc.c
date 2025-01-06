/*
 * base_sync.inc.c - Synchronization Primitives Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Mutex implementation
struct ppdb_base_mutex_s {
    pthread_mutex_t mutex;
    bool enable_stats;         // 运行时控制统计
    uint64_t lock_count;
    uint64_t contention_count;
    uint64_t total_wait_time_us;
    uint64_t max_wait_time_us;
};

ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex) {
    if (!mutex) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_mutex_t* m = (ppdb_base_mutex_t*)malloc(sizeof(ppdb_base_mutex_t));
    if (!m) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(m, 0, sizeof(ppdb_base_mutex_t));
    m->enable_stats = false;  // 默认禁用统计
    
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    
    if (pthread_mutex_init(&m->mutex, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        free(m);
        return PPDB_BASE_ERR_MUTEX;
    }
    
    pthread_mutexattr_destroy(&attr);
    *mutex = m;
    return PPDB_OK;
}

void ppdb_base_mutex_enable_stats(ppdb_base_mutex_t* mutex, bool enable) {
    if (mutex) {
        mutex->enable_stats = enable;
        if (!enable) {
            // 重置统计数据
            mutex->lock_count = 0;
            mutex->contention_count = 0;
            mutex->total_wait_time_us = 0;
            mutex->max_wait_time_us = 0;
        }
    }
}

ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_BASE_ERR_PARAM;
    }

    uint64_t start_time = ppdb_base_get_time_us();
    if (mutex->enable_stats) {
        start_time = ppdb_base_get_time_us();
    }

    int result = pthread_mutex_lock(&mutex->mutex);
    if (result != 0) {
        return PPDB_BASE_ERR_MUTEX;
    }

    if (mutex->enable_stats) {
        uint64_t end_time = ppdb_base_get_time_us();
        uint64_t wait_time = end_time - start_time;

        mutex->lock_count++;
        if (wait_time > 0) {
            mutex->contention_count++;
            mutex->total_wait_time_us += wait_time;
            if (wait_time > mutex->max_wait_time_us) {
                mutex->max_wait_time_us = wait_time;
            }
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_trylock(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_BASE_ERR_PARAM;
    }

    int result = pthread_mutex_trylock(&mutex->mutex);
    if (result == 0) {
        if (mutex->enable_stats) {
            mutex->lock_count++;
        }
        return PPDB_OK;
    } else if (result == EBUSY) {
        return PPDB_ERR_BUSY;
    }

    return PPDB_BASE_ERR_MUTEX;
}

ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        return PPDB_BASE_ERR_MUTEX;
    }

    return PPDB_OK;
}

void ppdb_base_mutex_get_stats(ppdb_base_mutex_t* mutex, uint64_t* lock_count,
                              uint64_t* contention_count, uint64_t* total_wait_time_us,
                              uint64_t* max_wait_time_us) {
    if (!mutex || !mutex->enable_stats) {
        if (lock_count) *lock_count = 0;
        if (contention_count) *contention_count = 0;
        if (total_wait_time_us) *total_wait_time_us = 0;
        if (max_wait_time_us) *max_wait_time_us = 0;
        return;
    }

    if (lock_count) *lock_count = mutex->lock_count;
    if (contention_count) *contention_count = mutex->contention_count;
    if (total_wait_time_us) *total_wait_time_us = mutex->total_wait_time_us;
    if (max_wait_time_us) *max_wait_time_us = mutex->max_wait_time_us;
}

const char* ppdb_base_mutex_get_error(ppdb_base_mutex_t* mutex) {
    (void)mutex; // Unused parameter
    return strerror(errno);
}

ppdb_error_t ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (pthread_mutex_destroy(&mutex->mutex) != 0) {
        return PPDB_BASE_ERR_MUTEX;
    }

    free(mutex);
    return PPDB_OK;
}

// Sync implementation
ppdb_error_t ppdb_base_sync_create(ppdb_base_sync_t** sync, const ppdb_base_sync_config_t* config) {
    if (!sync || !config) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_sync_t* s = (ppdb_base_sync_t*)malloc(sizeof(ppdb_base_sync_t));
    if (!s) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(s, 0, sizeof(ppdb_base_sync_t));
    s->config = *config;
    
    ppdb_error_t err = ppdb_base_mutex_create(&s->mutex);
    if (err != PPDB_OK) {
        free(s);
        return err;
    }

    *sync = s;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_sync_destroy(ppdb_base_sync_t* sync) {
    if (!sync) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (sync->mutex) {
        ppdb_base_mutex_destroy(sync->mutex);
    }
    free(sync);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_sync_lock(ppdb_base_sync_t* sync) {
    if (!sync) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (!sync->config.thread_safe) {
        return PPDB_OK;
    }

    return ppdb_base_mutex_lock(sync->mutex);
}

ppdb_error_t ppdb_base_sync_unlock(ppdb_base_sync_t* sync) {
    if (!sync) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (!sync->config.thread_safe) {
        return PPDB_OK;
    }

    return ppdb_base_mutex_unlock(sync->mutex);
}

ppdb_error_t ppdb_base_sync_init(ppdb_base_t* base) {
    if (!base) {
        return PPDB_BASE_ERR_PARAM;
    }

    base->sync_config.thread_safe = true;
    base->sync_config.spin_count = 1000;
    base->sync_config.backoff_us = 100;

    return PPDB_OK;
}

void ppdb_base_sync_cleanup(ppdb_base_t* base) {
    // Nothing to clean up
    (void)base;
}

// Performance test implementation
ppdb_error_t ppdb_base_sync_perf_test(ppdb_base_sync_t* sync, uint32_t num_threads, uint32_t iterations) {
    if (!sync || num_threads == 0 || iterations == 0) {
        return PPDB_BASE_ERR_PARAM;
    }

    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
    if (!threads) {
        return PPDB_BASE_ERR_MEMORY;
    }

    struct thread_data {
        ppdb_base_sync_t* sync;
        uint32_t iterations;
        uint64_t total_time;
    };

    struct thread_data* thread_data = (struct thread_data*)malloc(sizeof(struct thread_data) * num_threads);
    if (!thread_data) {
        free(threads);
        return PPDB_BASE_ERR_MEMORY;
    }

    void* thread_func(void* arg) {
        struct thread_data* data = (struct thread_data*)arg;
        uint64_t start_time = ppdb_base_get_time_us();
        
        for (uint32_t i = 0; i < data->iterations; i++) {
            ppdb_base_sync_lock(data->sync);
            // Simulate some work
            usleep(1);
            ppdb_base_sync_unlock(data->sync);
        }
        
        data->total_time = ppdb_base_get_time_us() - start_time;
        return NULL;
    }

    // Initialize thread data
    for (uint32_t i = 0; i < num_threads; i++) {
        thread_data[i].sync = sync;
        thread_data[i].iterations = iterations;
        thread_data[i].total_time = 0;
    }

    // Start threads
    for (uint32_t i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, thread_func, &thread_data[i]) != 0) {
            free(threads);
            free(thread_data);
            return PPDB_BASE_ERR_THREAD;
        }
    }

    // Wait for threads to complete
    for (uint32_t i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Calculate and print performance statistics
    uint64_t total_time = 0;
    uint64_t max_time = 0;
    for (uint32_t i = 0; i < num_threads; i++) {
        total_time += thread_data[i].total_time;
        if (thread_data[i].total_time > max_time) {
            max_time = thread_data[i].total_time;
        }
    }

    uint64_t avg_time = total_time / num_threads;
    printf("Sync Performance Test Results:\n");
    printf("  Number of threads: %u\n", num_threads);
    printf("  Iterations per thread: %u\n", iterations);
    printf("  Average time per thread: %lu us\n", avg_time);
    printf("  Max time per thread: %lu us\n", max_time);
    printf("  Total operations: %u\n", num_threads * iterations);
    printf("  Operations per second: %.2f\n", 
           (double)(num_threads * iterations) / ((double)max_time / 1000000.0));

    free(threads);
    free(thread_data);
    return PPDB_OK;
}

// Thread implementation
struct ppdb_base_thread_s {
    pthread_t thread;
    ppdb_base_thread_func_t func;
    void* arg;
    bool detached;
    uint64_t start_time;
    uint64_t wall_time;
    int state;
};

static void* thread_wrapper(void* arg) {
    ppdb_base_thread_t* t = (ppdb_base_thread_t*)arg;
    t->func(t->arg);
    t->wall_time = ppdb_base_get_time_us() - t->start_time;
    return NULL;
}

ppdb_error_t ppdb_base_thread_create(ppdb_base_thread_t** thread, ppdb_base_thread_func_t func, void* arg) {
    if (!thread || !func) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_thread_t* t = (ppdb_base_thread_t*)malloc(sizeof(ppdb_base_thread_t));
    if (!t) {
        return PPDB_BASE_ERR_MEMORY;
    }

    t->func = func;
    t->arg = arg;
    t->detached = false;
    t->start_time = ppdb_base_get_time_us();
    t->wall_time = 0;
    t->state = 0;

    if (pthread_create(&t->thread, NULL, thread_wrapper, t) != 0) {
        free(t);
        return PPDB_BASE_ERR_THREAD;
    }

    *thread = t;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_join(ppdb_base_thread_t* thread, void** retval) {
    if (!thread) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (thread->detached) {
        return PPDB_BASE_ERR_INVALID_STATE;
    }

    if (pthread_join(thread->thread, retval) != 0) {
        return PPDB_BASE_ERR_THREAD;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_detach(ppdb_base_thread_t* thread) {
    if (!thread) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (thread->detached) {
        return PPDB_BASE_ERR_INVALID_STATE;
    }

    if (pthread_detach(thread->thread) != 0) {
        return PPDB_BASE_ERR_THREAD;
    }

    thread->detached = true;
    return PPDB_OK;
}

void ppdb_base_thread_destroy(ppdb_base_thread_t* thread) {
    if (!thread) {
        return;
    }
    free(thread);
}

int ppdb_base_thread_get_state(ppdb_base_thread_t* thread) {
    if (!thread) {
        return -1;
    }
    return thread->state;
}

uint64_t ppdb_base_thread_get_wall_time(ppdb_base_thread_t* thread) {
    if (!thread) {
        return 0;
    }
    if (thread->wall_time == 0) {
        // 线程还在运行，返回当前运行时间
        return ppdb_base_get_time_us() - thread->start_time;
    }
    return thread->wall_time;
}

const char* ppdb_base_thread_get_error(ppdb_base_thread_t* thread) {
    (void)thread; // Unused parameter
    return strerror(errno);
}

// Spinlock implementation
ppdb_error_t ppdb_base_spinlock_create(ppdb_base_spinlock_t** spinlock) {
    if (!spinlock) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_spinlock_t* s = (ppdb_base_spinlock_t*)malloc(sizeof(ppdb_base_spinlock_t));
    if (!s) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(s, 0, sizeof(ppdb_base_spinlock_t));
    atomic_init(&s->lock, 0);
    s->enable_stats = false;  // 默认禁用统计
    s->spin_count = 1000;     // 默认自旋次数

    *spinlock = s;
    return PPDB_OK;
}

void ppdb_base_spinlock_enable_stats(ppdb_base_spinlock_t* spinlock, bool enable) {
    if (spinlock) {
        spinlock->enable_stats = enable;
        if (!enable) {
            // 重置统计数据
            spinlock->lock_count = 0;
            spinlock->contention_count = 0;
            spinlock->total_wait_time_us = 0;
            spinlock->max_wait_time_us = 0;
        }
    }
}

ppdb_error_t ppdb_base_spinlock_lock(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock) {
        return PPDB_BASE_ERR_PARAM;
    }

    uint64_t start_time = ppdb_base_get_time_us();
    if (spinlock->enable_stats) {
        start_time = ppdb_base_get_time_us();
    }

    int expected = 0;
    uint32_t spins = 0;
    
    while (!atomic_compare_exchange_weak(&spinlock->lock, &expected, 1)) {
        expected = 0;
        spins++;
        
        if (spins >= spinlock->spin_count) {
            // 超过自旋次数，让出CPU
            sched_yield();
            spins = 0;
        } else {
            // 短暂延迟
            __asm__ __volatile__("pause" ::: "memory");
        }
    }

    if (spinlock->enable_stats) {
        uint64_t end_time = ppdb_base_get_time_us();
        uint64_t wait_time = end_time - start_time;

        spinlock->lock_count++;
        if (wait_time > 0) {
            spinlock->contention_count++;
            spinlock->total_wait_time_us += wait_time;
            if (wait_time > spinlock->max_wait_time_us) {
                spinlock->max_wait_time_us = wait_time;
            }
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_spinlock_trylock(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock) {
        return PPDB_BASE_ERR_PARAM;
    }

    int expected = 0;
    if (atomic_compare_exchange_strong(&spinlock->lock, &expected, 1)) {
        if (spinlock->enable_stats) {
            spinlock->lock_count++;
        }
        return PPDB_OK;
    }

    return PPDB_ERR_BUSY;
}

void ppdb_base_spinlock_get_stats(ppdb_base_spinlock_t* spinlock, uint64_t* lock_count,
                                 uint64_t* contention_count, uint64_t* total_wait_time_us,
                                 uint64_t* max_wait_time_us) {
    if (!spinlock || !spinlock->enable_stats) {
        if (lock_count) *lock_count = 0;
        if (contention_count) *contention_count = 0;
        if (total_wait_time_us) *total_wait_time_us = 0;
        if (max_wait_time_us) *max_wait_time_us = 0;
        return;
    }

    if (lock_count) *lock_count = spinlock->lock_count;
    if (contention_count) *contention_count = spinlock->contention_count;
    if (total_wait_time_us) *total_wait_time_us = spinlock->total_wait_time_us;
    if (max_wait_time_us) *max_wait_time_us = spinlock->max_wait_time_us;
}

const char* ppdb_base_spinlock_get_error(ppdb_base_spinlock_t* spinlock) {
    (void)spinlock; // Unused parameter
    return strerror(errno);
}

void ppdb_base_spinlock_destroy(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock) {
        return;
    }
    free(spinlock);
}

ppdb_error_t ppdb_base_spinlock_unlock(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock) {
        return PPDB_BASE_ERR_PARAM;
    }

    atomic_store(&spinlock->lock, 0);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_spinlock_init(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock) {
        return PPDB_BASE_ERR_PARAM;
    }

    atomic_init(&spinlock->lock, 0);
    spinlock->enable_stats = false;
    spinlock->lock_count = 0;
    spinlock->contention_count = 0;
    spinlock->total_wait_time_us = 0;
    spinlock->max_wait_time_us = 0;
    spinlock->spin_count = 1000;  // Default spin count

    return PPDB_OK;
}