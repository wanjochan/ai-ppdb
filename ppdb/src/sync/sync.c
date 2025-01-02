#include <cosmopolitan.h>
#include "ppdb/sync.h"

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
static ppdb_error_t ppdb_sync_lockfree_put_once(void* args) {
    put_args_t* put_args = (put_args_t*)args;
    uint32_t hash = ppdb_sync_hash(put_args->key, put_args->key_len);
    
    // 使用CAS操作尝试获取锁
    uint32_t expected = 0;
    uint32_t desired = 1;
    
    // 尝试获取锁
    if (atomic_compare_exchange_strong(&put_args->atomic_lock, &expected, desired)) {
        // 成功获取锁，返回成功
        return PPDB_OK;
    }
    
    return PPDB_ERR_BUSY;
}

static ppdb_error_t ppdb_sync_lockfree_get_once(void* args) {
    get_args_t* get_args = (get_args_t*)args;
    uint32_t hash = ppdb_sync_hash(get_args->key, get_args->key_len);
    // TODO: 实现无锁读取逻辑
    return PPDB_ERR_RETRY;
}

static ppdb_error_t ppdb_sync_lockfree_delete_once(void* args) {
    delete_args_t* delete_args = (delete_args_t*)args;
    uint32_t hash = ppdb_sync_hash(delete_args->key, delete_args->key_len);
    // TODO: 实现无锁删除逻辑
    return PPDB_ERR_RETRY;
}

// 重试逻辑
ppdb_error_t ppdb_sync_retry(ppdb_sync_t* sync,
                            ppdb_sync_config_t* config,
                            ppdb_error_t (*retry_func)(void*),
                            void* args) {
    uint32_t retries = 0;
    uint32_t spins = 0;
    ppdb_error_t err;

    do {
        err = retry_func(args);
        if (err != PPDB_ERR_RETRY && err != PPDB_ERR_BUSY) {
            return err;
        }
        
        // 根据错误类型选择不同的等待策略
        if (err == PPDB_ERR_BUSY) {
            // 先进行自旋等待
            if (spins < config->spin_count) {
                ppdb_sync_pause();
                spins++;
                continue;
            }
            
            // 自旋次数达到阈值后进行退避
            ppdb_sync_backoff(config->backoff_us);
            spins = 0;
        } else {
            // 重试等待
            ppdb_sync_backoff(config->retry_delay_us);
        }
        
        retries++;
    } while (retries < config->retry_count);

    return PPDB_ERR_SYNC_RETRY_FAILED;
}

// 无锁操作接口
ppdb_error_t ppdb_sync_lockfree_put(ppdb_sync_t* sync,
                                   void* key,
                                   size_t key_len,
                                   void* value,
                                   size_t value_len,
                                   ppdb_sync_config_t* config) {
    put_args_t args = {
        .key = key,
        .key_len = key_len,
        .value = value,
        .value_len = value_len
    };
    return ppdb_sync_retry(sync, config, ppdb_sync_lockfree_put_once, &args);
}

ppdb_error_t ppdb_sync_lockfree_get(ppdb_sync_t* sync,
                                   void* key,
                                   size_t key_len,
                                   void** value,
                                   size_t* value_len,
                                   ppdb_sync_config_t* config) {
    get_args_t args = {
        .key = key,
        .key_len = key_len,
        .value = value,
        .value_len = value_len
    };
    return ppdb_sync_retry(sync, config, ppdb_sync_lockfree_get_once, &args);
}

ppdb_error_t ppdb_sync_lockfree_delete(ppdb_sync_t* sync,
                                      void* key,
                                      size_t key_len,
                                      ppdb_sync_config_t* config) {
    delete_args_t args = {
        .key = key,
        .key_len = key_len
    };
    return ppdb_sync_retry(sync, config, ppdb_sync_lockfree_delete_once, &args);
}

// 哈希函数
uint32_t ppdb_sync_hash(const void* data, size_t len) {
    // FNV-1a hash
    uint32_t hash = 2166136261u;
    const uint8_t* bytes = (const uint8_t*)data;
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

// 基本同步操作
ppdb_sync_t* ppdb_sync_create(void) {
    ppdb_sync_t* sync = (ppdb_sync_t*)malloc(sizeof(ppdb_sync_t));
    if (!sync) {
        return NULL;
    }
    ppdb_sync_config_t config = PPDB_SYNC_CONFIG_DEFAULT;
    if (ppdb_sync_init(sync, &config) != PPDB_OK) {
        free(sync);
        return NULL;
    }
    return sync;
}

ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config) {
    if (!sync || !config) {
        return PPDB_ERR_INVALID_PARAM;
    }

    pthread_mutexattr_t attr;
    int ret;

    // 初始化基本字段
    sync->type = config->type;
    sync->use_lockfree = config->use_lockfree;
    sync->spin_count = config->spin_count;
    sync->backoff_us = config->backoff_us;
    sync->enable_ref_count = config->enable_ref_count;
    sync->max_readers = config->max_readers;
    sync->enable_fairness = config->enable_fairness;

    // 初始化原子字段
    atomic_init(&sync->ref_count, 0);
    atomic_init(&sync->total_waiters, 0);
    atomic_flag_clear(&sync->is_contended);

    // 根据类型初始化同步原语
    switch (config->type) {
        case PPDB_SYNC_MUTEX:
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            ret = pthread_mutex_init(&sync->mutex, &attr);
            pthread_mutexattr_destroy(&attr);
            if (ret != 0) {
                return PPDB_ERR_INIT_FAILED;
            }
            break;

        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;

        case PPDB_SYNC_RWLOCK:
            atomic_init(&sync->rwlock.readers, 0);
            atomic_flag_clear(&sync->rwlock.writer);
            atomic_init(&sync->rwlock.waiting_writers, 0);
            atomic_init(&sync->rwlock.waiting_readers, 0);
            atomic_init(&sync->rwlock.atomic_lock, 0);
            break;

        default:
            return PPDB_ERR_INVALID_TYPE;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }
    
    if (!sync->use_lockfree) {
        if (pthread_mutex_destroy(&sync->mutex) != 0) {
            return PPDB_ERR_DESTROY_FAILED;
        }
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }

    uint32_t spins = 0;
    ppdb_error_t err;

    do {
        switch (sync->type) {
            case PPDB_SYNC_MUTEX:
                if (!sync->use_lockfree) {
                    int ret = pthread_mutex_trylock(&sync->mutex);
                    if (ret == 0) {
                        return PPDB_OK;
                    } else if (ret == EBUSY) {
                        err = PPDB_ERR_BUSY;
                    } else {
                        return PPDB_ERR_LOCK_FAILED;
                    }
                } else {
                    // 无锁模式下使用原子操作
                    uint32_t expected = 0;
                    uint32_t desired = 1;
                    if (atomic_compare_exchange_strong(&sync->rwlock.atomic_lock, &expected, desired)) {
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
                // 检查是否有读者或写者
                if (atomic_load(&sync->rwlock.readers) == 0 && 
                    !atomic_flag_test_and_set(&sync->rwlock.writer)) {
                    // 如果启用了公平性，检查是否有等待的写者
                    if (sync->enable_fairness && 
                        atomic_load(&sync->rwlock.waiting_writers) > 0) {
                        atomic_flag_clear(&sync->rwlock.writer);
                        err = PPDB_ERR_BUSY;
                    } else {
                        return PPDB_OK;
                    }
                } else {
                    err = PPDB_ERR_BUSY;
                }
                break;

            default:
                return PPDB_ERR_INVALID_TYPE;
        }

        // 如果操作失败，进行退避
        if (err == PPDB_ERR_BUSY) {
            if (ppdb_sync_should_yield(sync->spin_count, spins)) {
                ppdb_sync_backoff(sync->backoff_us);
                spins = 0;
            } else {
                ppdb_sync_pause();
                spins++;
            }
        }
    } while (err == PPDB_ERR_BUSY);

    return err;
}

ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }

    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            if (!sync->use_lockfree) {
                int ret = pthread_mutex_unlock(&sync->mutex);
                return ret == 0 ? PPDB_OK : PPDB_ERR_UNLOCK_FAILED;
            } else {
                // 无锁模式下使用原子操作
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
            return PPDB_ERR_INVALID_TYPE;
    }
}

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }

    if (sync->type != PPDB_SYNC_RWLOCK) {
        return PPDB_ERR_INVALID_TYPE;
    }

    // 增加等待读者计数
    atomic_fetch_add(&sync->rwlock.waiting_readers, 1);

    uint32_t spins = 0;
    while (true) {
        // 检查是否达到最大读者数量
        if (sync->max_readers > 0 && 
            atomic_load(&sync->rwlock.readers) >= sync->max_readers) {
            ppdb_sync_backoff(sync->backoff_us);
            continue;
        }

        // 如果启用了公平性且有等待的写者，需要等待
        if (sync->enable_fairness && 
            atomic_load(&sync->rwlock.waiting_writers) > 0) {
            if (ppdb_sync_should_yield(sync->spin_count, spins)) {
                ppdb_sync_backoff(sync->backoff_us);
                spins = 0;
            } else {
                ppdb_sync_pause();
                spins++;
            }
            continue;
        }

        // 尝试获取读锁
        if (!atomic_flag_test_and_set(&sync->rwlock.writer)) {
            atomic_fetch_add(&sync->rwlock.readers, 1);
            atomic_flag_clear(&sync->rwlock.writer);
            atomic_fetch_sub(&sync->rwlock.waiting_readers, 1);
            return PPDB_OK;
        }

        // 退避
        if (ppdb_sync_should_yield(sync->spin_count, spins)) {
            ppdb_sync_backoff(sync->backoff_us);
            spins = 0;
        } else {
            ppdb_sync_pause();
            spins++;
        }
    }
}

ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }

    if (sync->type != PPDB_SYNC_RWLOCK) {
        return PPDB_ERR_INVALID_TYPE;
    }

    // 减少读者计数
    atomic_fetch_sub(&sync->rwlock.readers, 1);

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }

    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_lock(&sync->mutex) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            return PPDB_OK;

        case PPDB_SYNC_RWLOCK:
            // 增加等待写者计数
            atomic_fetch_add(&sync->rwlock.waiting_writers, 1);

            uint32_t spins = 0;
            while (true) {
                // 尝试获取写锁
                if (!atomic_flag_test_and_set(&sync->rwlock.writer)) {
                    // 等待所有读者完成
                    while (atomic_load(&sync->rwlock.readers) > 0) {
                        if (ppdb_sync_should_yield(sync->spin_count, spins)) {
                            ppdb_sync_backoff(sync->backoff_us);
                            spins = 0;
                        } else {
                            ppdb_sync_pause();
                            spins++;
                        }
                    }
                    atomic_fetch_sub(&sync->rwlock.waiting_writers, 1);
                    return PPDB_OK;
                }

                // 退避
                if (ppdb_sync_should_yield(sync->spin_count, spins)) {
                    ppdb_sync_backoff(sync->backoff_us);
                    spins = 0;
                } else {
                    ppdb_sync_pause();
                    spins++;
                }
            }

        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }

    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_unlock(&sync->mutex) != 0) {
                return PPDB_ERR_UNLOCK_FAILED;
            }
            return PPDB_OK;

        case PPDB_SYNC_RWLOCK:
            // 释放写者标志
            atomic_flag_clear(&sync->rwlock.writer);
            return PPDB_OK;

        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

// 共享读锁
ppdb_error_t ppdb_sync_read_lock_shared(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }

    if (sync->type != PPDB_SYNC_RWLOCK) {
        return PPDB_ERR_INVALID_TYPE;
    }

    if (!sync->enable_ref_count) {
        return ppdb_sync_read_lock(sync);
    }

    // 等待直到没有写者
    uint32_t spin_count = 0;
    while (atomic_flag_test_and_set(&sync->rwlock.writer)) {
        atomic_flag_clear(&sync->rwlock.writer);  // 立即释放，因为我们只是在测试
        if (spin_count++ > sync->spin_count) {
            usleep(sync->backoff_us);
            spin_count = 0;
        } else {
            __asm__ volatile("pause");
        }
    }
    atomic_flag_clear(&sync->rwlock.writer);  // 释放标志，因为我们只是在测试

    // 增加读者计数和引用计数
    atomic_fetch_add(&sync->rwlock.readers, 1);
    atomic_fetch_add(&sync->ref_count, 1);

    return PPDB_OK;
}

// 共享读锁释放
ppdb_error_t ppdb_sync_read_unlock_shared(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }

    if (sync->type != PPDB_SYNC_RWLOCK) {
        return PPDB_ERR_INVALID_TYPE;
    }

    if (!sync->enable_ref_count) {
        return ppdb_sync_read_unlock(sync);
    }

    // 减少读者计数和引用计数
    atomic_fetch_sub(&sync->rwlock.readers, 1);
    atomic_fetch_sub(&sync->ref_count, 1);

    return PPDB_OK;
} 