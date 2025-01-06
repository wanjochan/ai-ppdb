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

ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex) {
    if (!mutex) return PPDB_ERR_PARAM;

    ppdb_base_mutex_t* m = (ppdb_base_mutex_t*)ppdb_base_aligned_alloc(16, sizeof(ppdb_base_mutex_t));
    if (!m) return PPDB_ERR_MEMORY;

    pthread_mutex_init(&m->mutex, NULL);  // Cosmopolitan provides pthread
    m->initialized = true;
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

    if (pthread_mutex_lock(&mutex->mutex) != 0) {
        return PPDB_ERR_INVALID_STATE;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_PARAM;
    if (!mutex->initialized) return PPDB_ERR_INVALID_STATE;

    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        return PPDB_ERR_INVALID_STATE;
    }
    return PPDB_OK;
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

ppdb_error_t ppdb_base_spinlock_create(ppdb_base_spinlock_t** spinlock) {
    if (!spinlock) return PPDB_ERR_PARAM;

    ppdb_base_spinlock_t* s = (ppdb_base_spinlock_t*)ppdb_base_aligned_alloc(16, sizeof(ppdb_base_spinlock_t));
    if (!s) return PPDB_ERR_MEMORY;

    atomic_flag_clear(&s->flag);
    s->initialized = true;
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

    while (atomic_flag_test_and_set(&spinlock->flag)) {
        // 自旋等待
        __builtin_ia32_pause();
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_spinlock_unlock(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock) return PPDB_ERR_PARAM;
    if (!spinlock->initialized) return PPDB_ERR_INVALID_STATE;

    atomic_flag_clear(&spinlock->flag);
    return PPDB_OK;
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
};

static void* thread_wrapper(void* arg) {
    ppdb_base_thread_t* t = (ppdb_base_thread_t*)arg;
    void* result = t->func(t->arg);
    return result;
}

ppdb_error_t ppdb_base_thread_create(ppdb_base_thread_t** thread, ppdb_base_thread_func_t func, void* arg) {
    if (!thread || !func) return PPDB_ERR_PARAM;

    ppdb_base_thread_t* t = (ppdb_base_thread_t*)ppdb_base_aligned_alloc(16, sizeof(ppdb_base_thread_t));
    if (!t) return PPDB_ERR_MEMORY;

    memset(t, 0, sizeof(ppdb_base_thread_t));
    t->func = func;
    t->arg = arg;
    t->initialized = true;
    t->joined = false;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // 直接调用函数，而不是创建线程
    func(arg);
    t->joined = true;

    pthread_attr_destroy(&attr);
    *thread = t;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_join(ppdb_base_thread_t* thread, void** retval) {
    if (!thread) return PPDB_ERR_PARAM;
    if (!thread->initialized) return PPDB_ERR_INVALID_STATE;
    if (thread->joined) return PPDB_ERR_INVALID_STATE;

    // 线程已经在创建时执行完毕
    if (retval) {
        *retval = NULL;
    }

    thread->joined = true;
    return PPDB_OK;
}

void ppdb_base_thread_destroy(ppdb_base_thread_t* thread) {
    if (!thread) return;
    ppdb_base_aligned_free(thread);
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