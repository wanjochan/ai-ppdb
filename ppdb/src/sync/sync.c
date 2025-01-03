#include <cosmopolitan.h>
#include "ppdb/sync.h"

#define PPDB_WRITE_BIT    (1U << 31)
#define PPDB_READER_MASK  (0x7FFFFFFF)
#define PPDB_READER_INC   (1)

static inline bool ppdb_rwlock_has_writer(int state) {
    return (state & PPDB_WRITE_BIT) != 0;
}

static inline int ppdb_rwlock_reader_count(int state) {
    return state & PPDB_READER_MASK;
}

static void ppdb_sync_backoff(uint32_t us) {
    if (us > 0) {
        usleep(us);
    }
}

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    if (sync->config.type != PPDB_SYNC_RWLOCK) return PPDB_ERR_INVALID_STATE;

    uint32_t retries = 0;
    while (retries++ < sync->config.max_retries) {
        int state = atomic_load(&sync->rwlock.state);
        
        if (!ppdb_rwlock_has_writer(state) && 
            ppdb_rwlock_reader_count(state) < sync->config.max_readers) {
            
            if (atomic_compare_exchange_weak(&sync->rwlock.state, 
                                          &state, 
                                          state + PPDB_READER_INC)) {
                atomic_fetch_add(&sync->stats.read_locks, 1);
        return PPDB_OK;
    }
            atomic_fetch_add(&sync->stats.contentions, 1);
        }
        
        ppdb_sync_backoff(sync->config.backoff_us);
    }
    
    atomic_fetch_add(&sync->stats.read_timeouts, 1);
    return PPDB_ERR_BUSY;
}

ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    if (sync->config.type != PPDB_SYNC_RWLOCK) return PPDB_ERR_INVALID_STATE;

    uint32_t retries = 0;
    while (retries++ < sync->config.max_retries) {
        int state = atomic_load(&sync->rwlock.state);
        
        if (state == 0) {
            if (atomic_compare_exchange_weak(&sync->rwlock.state,
                                          &state,
                                          PPDB_WRITE_BIT)) {
                atomic_fetch_add(&sync->stats.write_locks, 1);
                return PPDB_OK;
            }
            atomic_fetch_add(&sync->stats.contentions, 1);
        }
        
        ppdb_sync_backoff(sync->config.backoff_us);
    }
    
    atomic_fetch_add(&sync->stats.write_timeouts, 1);
    return PPDB_ERR_BUSY;
}

ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    if (sync->config.type != PPDB_SYNC_RWLOCK) return PPDB_ERR_INVALID_STATE;

    int state = atomic_load(&sync->rwlock.state);
    if (ppdb_rwlock_reader_count(state) == 0) {
        return PPDB_ERR_INVALID_STATE;
    }

    atomic_fetch_sub(&sync->rwlock.state, PPDB_READER_INC);
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    if (sync->config.type != PPDB_SYNC_RWLOCK) return PPDB_ERR_INVALID_STATE;

    int state = atomic_load(&sync->rwlock.state);
    if (!ppdb_rwlock_has_writer(state)) {
        return PPDB_ERR_INVALID_STATE;
    }

    atomic_store(&sync->rwlock.state, 0);
        return PPDB_OK;
    }

ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, ppdb_sync_config_t* config) {
    if (!sync || !config) {
        return PPDB_ERR_NULL_POINTER;
    }

    *sync = (ppdb_sync_t*)malloc(sizeof(ppdb_sync_t));
    if (!*sync) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    ppdb_error_t err = ppdb_sync_init(*sync, config);
    if (err != PPDB_OK) {
        free(*sync);
        *sync = NULL;
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, ppdb_sync_config_t* config) {
    if (!sync || !config) {
        return PPDB_ERR_NULL_POINTER;
    }

    memset(sync, 0, sizeof(ppdb_sync_t));
    sync->config = *config;

    switch (config->type) {
        case PPDB_SYNC_MUTEX:
            if (!config->use_lockfree) {
                pthread_mutexattr_t attr;
                pthread_mutexattr_init(&attr);
                pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
                if (pthread_mutex_init(&sync->mutex, &attr) != 0) {
                    pthread_mutexattr_destroy(&attr);
                    return PPDB_ERR_INTERNAL;
                }
                pthread_mutexattr_destroy(&attr);
            }
            break;

        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;

        case PPDB_SYNC_RWLOCK:
            atomic_store(&sync->rwlock.state, 0);
            atomic_store(&sync->rwlock.waiters, 0);
            break;

        default:
            return PPDB_ERR_NOT_SUPPORTED;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_NULL_POINTER;
    }

    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (!sync->config.use_lockfree) {
                if (pthread_mutex_destroy(&sync->mutex) != 0) {
                    return PPDB_ERR_INTERNAL;
                }
            }
            break;

        case PPDB_SYNC_SPINLOCK:
        case PPDB_SYNC_RWLOCK:
            // 这些类型不需要特殊的清理
            break;

        default:
            return PPDB_ERR_NOT_SUPPORTED;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_NULL_POINTER;
    }

    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (!sync->config.use_lockfree) {
                int ret = pthread_mutex_trylock(&sync->mutex);
                if (ret == 0) {
                    return PPDB_OK;
                }
                return (ret == EBUSY) ? PPDB_ERR_BUSY : PPDB_ERR_INTERNAL;
            }
            // 无锁模式使用自旋锁
            /* fall through */

        case PPDB_SYNC_SPINLOCK:
            if (!atomic_flag_test_and_set(&sync->spinlock)) {
                return PPDB_OK;
            }
            return PPDB_ERR_BUSY;

        case PPDB_SYNC_RWLOCK:
            return ppdb_sync_write_lock(sync);

        default:
            return PPDB_ERR_NOT_SUPPORTED;
    }
}

ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_NULL_POINTER;
    }

    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (!sync->config.use_lockfree) {
                int ret = pthread_mutex_unlock(&sync->mutex);
                return ret == 0 ? PPDB_OK : PPDB_ERR_UNLOCK_FAILED;
            }
            // 无锁模式使用自旋锁
            /* fall through */

        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            return PPDB_OK;

        case PPDB_SYNC_RWLOCK:
            return ppdb_sync_write_unlock(sync);

        default:
            return PPDB_ERR_NOT_SUPPORTED;
    }
}

// ... 其他函数实现保持类似的更新模式 ... 