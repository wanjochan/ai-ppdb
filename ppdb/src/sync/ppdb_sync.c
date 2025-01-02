#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "ppdb/ppdb_sync.h"

// 无锁操作的参数结构
typedef struct {
    void* key;
    size_t key_len;
    void* value;
    size_t value_len;
    atomic_uint atomic_lock;
} put_args_t;

typedef struct {
    void* key;
    size_t key_len;
    void** value;
    size_t* value_len;
} get_args_t;

typedef struct {
    void* key;
    size_t key_len;
} delete_args_t;

// 内部工具函数实现
void ppdb_sync_pause(void) {
    __asm__ volatile("pause");
}

void ppdb_sync_backoff(uint32_t backoff_us) {
    usleep(backoff_us);
}

bool ppdb_sync_should_yield(uint32_t spin_count, uint32_t current_spins) {
    return current_spins >= spin_count;
}

// 无锁操作的单次尝试函数
static ppdb_error_t ppdb_sync_lockfree_put_once(void* arg) {
    ppdb_sync_lockfree_args_t* args = (ppdb_sync_lockfree_args_t*)arg;
    if (!args) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 尝试获取锁
    if (atomic_flag_test_and_set(&args->sync->spinlock)) {
        return PPDB_ERR_BUSY;
    }

    // 执行操作
    memcpy(args->value, args->key, args->key_len);
    memcpy(args->value + args->key_len, args->value_ptr, args->value_len);

    // 释放锁
    atomic_flag_clear(&args->sync->spinlock);
    return PPDB_OK;
}

static ppdb_error_t ppdb_sync_lockfree_get_once(void* arg) {
    ppdb_sync_lockfree_args_t* args = (ppdb_sync_lockfree_args_t*)arg;
    if (!args) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 尝试获取锁
    if (atomic_flag_test_and_set(&args->sync->spinlock)) {
        return PPDB_ERR_BUSY;
    }

    // 执行操作
    memcpy(args->value_ptr, args->value + args->key_len, args->value_len);

    // 释放锁
    atomic_flag_clear(&args->sync->spinlock);
    return PPDB_OK;
}

static ppdb_error_t ppdb_sync_lockfree_delete_once(void* arg) {
    ppdb_sync_lockfree_args_t* args = (ppdb_sync_lockfree_args_t*)arg;
    if (!args) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 尝试获取锁
    if (atomic_flag_test_and_set(&args->sync->spinlock)) {
        return PPDB_ERR_BUSY;
    }

    // 执行操作
    memset(args->value, 0, args->key_len);

    // 释放锁
    atomic_flag_clear(&args->sync->spinlock);
    return PPDB_OK;
}

// 重试逻辑
ppdb_error_t ppdb_sync_retry(ppdb_sync_t* sync, ppdb_sync_retry_func_t retry_func, void* arg) {
    if (!sync || !retry_func) {
        return PPDB_ERR_INVALID_ARG;
    }

    uint32_t retries = 0;
    const uint32_t MAX_RETRIES = 1000;
    ppdb_error_t err;

    do {
        err = retry_func(arg);
        if (err != PPDB_ERR_BUSY) {
            return err;
        }

        if (retries++ > MAX_RETRIES) {
            return PPDB_ERR_INTERNAL;
        }

        // 指数退避
        if (retries > 1) {
            uint32_t backoff = sync->backoff_us * (1 << (retries - 1));
            if (backoff > 1000000) { // 最大退避1秒
                backoff = 1000000;
            }
            ppdb_sync_backoff(backoff);
        }
    } while (true);

    return PPDB_ERR_INTERNAL;
}

// 无锁操作接口
ppdb_error_t ppdb_sync_lockfree_put(ppdb_sync_t* sync, void* key, size_t key_len, void* value, size_t value_len) {
    ppdb_sync_lockfree_args_t args = {
        .sync = sync,
        .key = key,
        .key_len = key_len,
        .value = value,
        .value_ptr = value,
        .value_len = value_len
    };
    return ppdb_sync_retry(sync, ppdb_sync_lockfree_put_once, &args);
}

ppdb_error_t ppdb_sync_lockfree_get(ppdb_sync_t* sync, void* key, size_t key_len, void* value, size_t value_len) {
    ppdb_sync_lockfree_args_t args = {
        .sync = sync,
        .key = key,
        .key_len = key_len,
        .value = value,
        .value_ptr = value,
        .value_len = value_len
    };
    return ppdb_sync_retry(sync, ppdb_sync_lockfree_get_once, &args);
}

ppdb_error_t ppdb_sync_lockfree_delete(ppdb_sync_t* sync, void* key, size_t key_len) {
    ppdb_sync_lockfree_args_t args = {
        .sync = sync,
        .key = key,
        .key_len = key_len,
        .value = NULL,
        .value_ptr = NULL,
        .value_len = 0
    };
    return ppdb_sync_retry(sync, ppdb_sync_lockfree_delete_once, &args);
}

// 基本同步操作
ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, ppdb_sync_config_t* config) {
    if (!sync || !config) {
        return PPDB_ERR_INVALID_ARG;
    }

    *sync = (ppdb_sync_t*)malloc(sizeof(ppdb_sync_t));
    if (!*sync) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    if (ppdb_sync_init(*sync, config) != PPDB_OK) {
        free(*sync);
        return PPDB_ERR_INTERNAL;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, ppdb_sync_config_t* config) {
    if (!sync || !config) {
        return PPDB_ERR_INVALID_ARG;
    }

    memset(sync, 0, sizeof(ppdb_sync_t));
    sync->type = config->type;
    sync->use_lockfree = config->use_lockfree;
    sync->enable_fairness = config->enable_fairness;
    sync->enable_ref_count = config->enable_ref_count;
    sync->spin_count = config->spin_count;
    sync->backoff_us = config->backoff_us;
    sync->max_readers = config->max_readers;

    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            if (!sync->use_lockfree) {
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
            atomic_store(&sync->rwlock.readers, 0);
            atomic_store(&sync->rwlock.waiting_writers, 0);
            atomic_flag_clear(&sync->rwlock.writer);
            atomic_store(&sync->rwlock.atomic_lock, 0);
            break;

        default:
            return PPDB_ERR_NOT_SUPPORTED;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            if (!sync->use_lockfree) {
                if (pthread_mutex_destroy(&sync->mutex) != 0) {
                    return PPDB_ERR_INTERNAL;
                }
            }
            break;

        case PPDB_SYNC_SPINLOCK:
            // 自旋锁不需要特殊清理
            break;

        case PPDB_SYNC_RWLOCK:
            // 确保没有读者或写者
            if (atomic_load(&sync->rwlock.readers) > 0 ||
                atomic_load(&sync->rwlock.waiting_writers) > 0 ||
                atomic_flag_test_and_set(&sync->rwlock.writer)) {
                return PPDB_ERR_BUSY;
            }
            atomic_flag_clear(&sync->rwlock.writer);
            break;

        default:
            return PPDB_ERR_NOT_SUPPORTED;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = PPDB_OK;

    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            if (!sync->use_lockfree) {
                int ret = pthread_mutex_trylock(&sync->mutex);
                if (ret == 0) {
                    return PPDB_OK;
                } else if (ret == EBUSY) {
                    err = PPDB_ERR_BUSY;
                } else {
                    return PPDB_ERR_INTERNAL;
                }
            } else {
                // 无锁模式下使用原子操作
                int expected = 0;
                if (atomic_compare_exchange_strong(&sync->rwlock.atomic_lock, &expected, 1)) {
                    return PPDB_OK;
                }
                err = PPDB_ERR_BUSY;
            }
            break;

        case PPDB_SYNC_SPINLOCK:
            if (!atomic_flag_test_and_set(&sync->spinlock)) {
                return PPDB_OK;
            }
            err = PPDB_ERR_BUSY;
            break;

        case PPDB_SYNC_RWLOCK:
            if (!atomic_flag_test_and_set(&sync->rwlock.writer)) {
                return PPDB_OK;
            }
            err = PPDB_ERR_BUSY;
            break;

        default:
            return PPDB_ERR_NOT_SUPPORTED;
    }

    return err;
}

ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            if (!sync->use_lockfree) {
                int ret = pthread_mutex_unlock(&sync->mutex);
                return ret == 0 ? PPDB_OK : PPDB_ERR_INTERNAL;
            } else {
                atomic_store(&sync->rwlock.atomic_lock, 0);
                return PPDB_OK;
            }
            break;

        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            return PPDB_OK;

        case PPDB_SYNC_RWLOCK:
            atomic_flag_clear(&sync->rwlock.writer);
            return PPDB_OK;

        default:
            return PPDB_ERR_NOT_SUPPORTED;
    }
}

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (sync->use_lockfree) {
        uint32_t spins = 0;
        uint32_t retries = 0;
        const uint32_t MAX_RETRIES = 1000;

        while (retries++ < MAX_RETRIES) {
            // 检查是否有写者持有锁
            if (atomic_flag_test_and_set(&sync->rwlock.writer)) {
                if (spins++ > sync->spin_count) {
                    ppdb_sync_backoff(sync->backoff_us);
                    spins = 0;
                }
                continue;
            }

            // 检查读者数量
            int readers = atomic_load(&sync->rwlock.readers);
            if (readers >= sync->max_readers) {
                atomic_flag_clear(&sync->rwlock.writer);
                return PPDB_ERR_INTERNAL;
            }

            // 尝试增加读者计数
            if (atomic_compare_exchange_strong(&sync->rwlock.readers, &readers, readers + 1)) {
                atomic_flag_clear(&sync->rwlock.writer);
                return PPDB_OK;
            }

            // 如果增加读者计数失败，释放写锁并重试
            atomic_flag_clear(&sync->rwlock.writer);

            if (spins++ > sync->spin_count) {
                ppdb_sync_backoff(sync->backoff_us);
                spins = 0;
            }
        }
        return PPDB_ERR_BUSY;
    } else {
        return pthread_mutex_lock(&sync->mutex) == 0 ? PPDB_OK : PPDB_ERR_INTERNAL;
    }
}

ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (sync->use_lockfree) {
        int readers = atomic_load(&sync->rwlock.readers);
        if (readers <= 0) {
            return PPDB_ERR_INTERNAL;  // 防止重复解锁
        }
        atomic_fetch_sub(&sync->rwlock.readers, 1);
        return PPDB_OK;
    } else {
        return pthread_mutex_unlock(&sync->mutex) == 0 ? PPDB_OK : PPDB_ERR_INTERNAL;
    }
}

ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (sync->use_lockfree) {
        uint32_t spins = 0;
        uint32_t retries = 0;
        const uint32_t MAX_RETRIES = 1000;
        
        // 增加等待写者计数
        atomic_fetch_add(&sync->rwlock.waiting_writers, 1);

        while (retries++ < MAX_RETRIES) {
            // 检查是否有读者
            if (atomic_load(&sync->rwlock.readers) > 0) {
                if (spins++ > sync->spin_count) {
                    ppdb_sync_backoff(sync->backoff_us);
                    spins = 0;
                }
                continue;
            }

            // 尝试获取写锁
            if (!atomic_flag_test_and_set(&sync->rwlock.writer)) {
                // 再次检查是否有读者（避免竞态条件）
                if (atomic_load(&sync->rwlock.readers) == 0) {
                    // 减少等待写者计数
                    atomic_fetch_sub(&sync->rwlock.waiting_writers, 1);
                    return PPDB_OK;
                }
                atomic_flag_clear(&sync->rwlock.writer);
            }

            if (spins++ > sync->spin_count) {
                ppdb_sync_backoff(sync->backoff_us);
                spins = 0;
            }
        }

        // 超时，减少等待写者计数并返回
        atomic_fetch_sub(&sync->rwlock.waiting_writers, 1);
        return PPDB_ERR_BUSY;
    } else {
        return pthread_mutex_lock(&sync->mutex) == 0 ? PPDB_OK : PPDB_ERR_INTERNAL;
    }
}

ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (sync->use_lockfree) {
        if (!atomic_flag_test_and_set(&sync->rwlock.writer)) {
            return PPDB_ERR_INTERNAL;  // 防止重复解锁
        }
        atomic_flag_clear(&sync->rwlock.writer);
        return PPDB_OK;
    } else {
        return pthread_mutex_unlock(&sync->mutex) == 0 ? PPDB_OK : PPDB_ERR_INTERNAL;
    }
}

ppdb_error_t ppdb_sync_read_lock_shared(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (sync->type != PPDB_SYNC_RWLOCK) {
        return PPDB_ERR_NOT_SUPPORTED;
    }

    if (!sync->enable_ref_count) {
        return ppdb_sync_read_lock(sync);
    }

    // 等待直到没有写者
    uint32_t spin_count = 0;
    while (atomic_flag_test_and_set(&sync->rwlock.writer)) {
        if (spin_count++ > sync->spin_count) {
            ppdb_sync_backoff(sync->backoff_us);
            spin_count = 0;
        } else {
            ppdb_sync_pause();
        }
    }

    // 增加读者计数
    atomic_fetch_add(&sync->rwlock.readers, 1);
    atomic_flag_clear(&sync->rwlock.writer);

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_read_unlock_shared(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (sync->type != PPDB_SYNC_RWLOCK) {
        return PPDB_ERR_NOT_SUPPORTED;
    }

    if (!sync->enable_ref_count) {
        return ppdb_sync_read_unlock(sync);
    }

    // 减少读者计数
    int readers = atomic_fetch_sub(&sync->rwlock.readers, 1) - 1;
    if (readers < 0) {
        // 恢复计数
        atomic_fetch_add(&sync->rwlock.readers, 1);
        return PPDB_ERR_INTERNAL;
    }

    return PPDB_OK;
} 