#ifndef PPDB_BASE_SYNC_INC_C
#define PPDB_BASE_SYNC_INC_C

#include <cosmopolitan.h>
#include <ppdb/internal.h>

#define SYNC_ALIGNMENT 16

// Time functions
static uint64_t time_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// Helper functions
static void sync_backoff(uint32_t backoff_us) {
    if (backoff_us == 0) return;
    struct timespec ts = {
        .tv_sec = backoff_us / 1000000,
        .tv_nsec = (backoff_us % 1000000) * 1000
    };
    nanosleep(&ts, NULL);
}

static void sync_update_stats(ppdb_sync_t* sync, uint64_t wait_time_us) {
    // TODO: Implement statistics collection
    (void)sync;
    (void)wait_time_us;
}

// Create sync object
ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, const ppdb_sync_config_t* config) {
    if (!sync || !config) return PPDB_ERR_NULL_POINTER;
    if (*sync) return PPDB_ERR_EXISTS;

    // Allocate sync object
    ppdb_sync_t* s = (ppdb_sync_t*)ppdb_aligned_alloc(SYNC_ALIGNMENT, sizeof(ppdb_sync_t));
    if (!s) return PPDB_ERR_OUT_OF_MEMORY;

    // Initialize sync object
    memset(s, 0, sizeof(ppdb_sync_t));
    s->config = *config;

    // Create mutex if thread safe
    if (config->thread_safe) {
        ppdb_error_t err = ppdb_core_mutex_create(&s->mutex);
        if (err != PPDB_OK) {
            ppdb_aligned_free(s);
            return err;
        }
    }

    *sync = s;
    return PPDB_OK;
}

// Destroy sync object
void ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) return;

    if (sync->mutex) {
        ppdb_core_mutex_destroy(sync->mutex);
    }

    ppdb_aligned_free(sync);
}

// Lock operations
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;

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

ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    if (!sync->writer) return PPDB_ERR_INVALID_STATE;

    sync->writer = false;
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;

    if (!sync->writer && __sync_bool_compare_and_swap(&sync->writer, false, true)) {
        return PPDB_OK;
    }

    return PPDB_ERR_BUSY;
}

// Read-write lock operations
ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;

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

ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    if (sync->readers == 0) return PPDB_ERR_INVALID_STATE;

    __sync_fetch_and_sub(&sync->readers, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_try_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;

    if (!sync->writer) {
        __sync_fetch_and_add(&sync->readers, 1);
        if (!sync->writer) {
            return PPDB_OK;
        }
        __sync_fetch_and_sub(&sync->readers, 1);
    }

    return PPDB_ERR_BUSY;
}

ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;

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

ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    if (!sync->writer) return PPDB_ERR_INVALID_STATE;

    sync->writer = false;
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_try_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;

    if (!sync->writer && sync->readers == 0 &&
        __sync_bool_compare_and_swap(&sync->writer, false, true)) {
        return PPDB_OK;
    }

    return PPDB_ERR_BUSY;
}

ppdb_error_t ppdb_core_mutex_create(ppdb_core_mutex_t** mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_t* m = (ppdb_core_mutex_t*)ppdb_aligned_alloc(16, sizeof(ppdb_core_mutex_t));
    if (!m) return PPDB_ERR_OUT_OF_MEMORY;

    if (pthread_mutex_init(&m->mutex, NULL) != 0) {
        ppdb_aligned_free(m);
        return PPDB_ERR_INVALID_STATE;
    }

    m->initialized = true;
    *mutex = m;
    return PPDB_OK;
}

void ppdb_core_mutex_destroy(ppdb_core_mutex_t* mutex) {
    if (!mutex) return;
    if (mutex->initialized) {
        pthread_mutex_destroy(&mutex->mutex);
    }
    ppdb_aligned_free(mutex);
}

ppdb_error_t ppdb_core_mutex_lock(ppdb_core_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;
    if (!mutex->initialized) return PPDB_ERR_INVALID_STATE;

    if (pthread_mutex_lock(&mutex->mutex) != 0) {
        return PPDB_ERR_INVALID_STATE;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_core_mutex_unlock(ppdb_core_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;
    if (!mutex->initialized) return PPDB_ERR_INVALID_STATE;

    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        return PPDB_ERR_INVALID_STATE;
    }
    return PPDB_OK;
}

#endif // PPDB_BASE_SYNC_INC_C 