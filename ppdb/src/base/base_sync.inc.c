/*
 * base_sync.inc.c - Synchronization Primitives Implementation
 */

#include <cosmopolitan.h>
#include "../internal/base.h"

// Mutex implementation
struct ppdb_base_mutex_s {
    pthread_mutex_t mutex;
    uint64_t lock_count;
    uint64_t contention_count;
    uint64_t total_wait_time_us;
    uint64_t max_wait_time_us;
};

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    ppdb_base_mutex_t* m = (ppdb_base_mutex_t*)malloc(sizeof(ppdb_base_mutex_t));
    if (!m) {
        return PPDB_ERR_MEMORY;
    }

    memset(m, 0, sizeof(ppdb_base_mutex_t));
    
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    
    if (pthread_mutex_init(&m->mutex, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        free(m);
        return PPDB_BASE_ERR_MUTEX;
    }
    
    pthread_mutexattr_destroy(&attr);
    *mutex = m;
    return PPDB_OK;
}

void ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return;
    }
    pthread_mutex_destroy(&mutex->mutex);
    free(mutex);
}

ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    uint64_t start_time = get_time_us();
    int result = pthread_mutex_lock(&mutex->mutex);
    uint64_t end_time = get_time_us();
    uint64_t wait_time = end_time - start_time;

    if (result != 0) {
        return PPDB_BASE_ERR_MUTEX;
    }

    mutex->lock_count++;
    if (wait_time > 0) {
        mutex->contention_count++;
        mutex->total_wait_time_us += wait_time;
        if (wait_time > mutex->max_wait_time_us) {
            mutex->max_wait_time_us = wait_time;
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_trylock(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    int result = pthread_mutex_trylock(&mutex->mutex);
    if (result == 0) {
        mutex->lock_count++;
        return PPDB_OK;
    } else if (result == EBUSY) {
        return PPDB_ERR_BUSY;
    }

    return PPDB_BASE_ERR_MUTEX;
}

ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        return PPDB_BASE_ERR_MUTEX;
    }

    return PPDB_OK;
}

void ppdb_base_mutex_get_stats(ppdb_base_mutex_t* mutex, uint64_t* lock_count,
                              uint64_t* contention_count, uint64_t* total_wait_time_us,
                              uint64_t* max_wait_time_us) {
    if (!mutex) {
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

// Sync implementation
ppdb_error_t ppdb_base_sync_create(ppdb_base_sync_t** sync, const ppdb_base_sync_config_t* config) {
    if (!sync || !config) {
        return PPDB_ERR_PARAM;
    }

    ppdb_base_sync_t* s = (ppdb_base_sync_t*)malloc(sizeof(ppdb_base_sync_t));
    if (!s) {
        return PPDB_ERR_MEMORY;
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
        return PPDB_ERR_PARAM;
    }

    if (sync->mutex) {
        ppdb_base_mutex_destroy(sync->mutex);
    }
    free(sync);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_sync_lock(ppdb_base_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_PARAM;
    }

    if (!sync->config.thread_safe) {
        return PPDB_OK;
    }

    return ppdb_base_mutex_lock(sync->mutex);
}

ppdb_error_t ppdb_base_sync_unlock(ppdb_base_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_PARAM;
    }

    if (!sync->config.thread_safe) {
        return PPDB_OK;
    }

    return ppdb_base_mutex_unlock(sync->mutex);
}

ppdb_error_t ppdb_base_sync_init(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_PARAM;
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