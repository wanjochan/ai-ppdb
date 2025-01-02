#include <cosmopolitan.h>
#include "ppdb/sync.h"
#include "internal/internal_sync.h"

// 无锁操作的参数结构
typedef struct {
    void* key;
    size_t key_len;
    void* value;
    size_t value_len;
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

// 无锁操作的单次尝试函数
static ppdb_error_t ppdb_sync_lockfree_put_once(void* args) {
    put_args_t* put_args = (put_args_t*)args;
    uint32_t hash = ppdb_sync_hash(put_args->key, put_args->key_len);
    // TODO: 实现无锁写入逻辑
    return PPDB_ERR_RETRY;
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
    ppdb_error_t err;

    do {
        err = retry_func(args);
        if (err != PPDB_ERR_RETRY) {
            return err;
        }
        usleep(config->retry_delay_us);
        retries++;
    } while (retries < config->retry_count);

    return PPDB_ERR_RETRY;
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
    
    // 初始化基本字段
    sync->type = config->type;
    sync->use_lockfree = config->use_lockfree;
    sync->spin_count = config->spin_count;
    sync->backoff_us = config->backoff_us;
    sync->enable_ref_count = config->enable_ref_count;
    atomic_init(&sync->ref_count, 0);
    
    // 根据类型初始化同步原语
    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            if (!sync->use_lockfree) {
                pthread_mutexattr_t attr;
                pthread_mutexattr_init(&attr);
                pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
                if (pthread_mutex_init(&sync->mutex, &attr) != 0) {
                    pthread_mutexattr_destroy(&attr);
                    return PPDB_ERR_INIT_FAILED;
                }
                pthread_mutexattr_destroy(&attr);
            } else {
                atomic_flag_clear(&sync->spinlock);
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;
            
        case PPDB_SYNC_RWLOCK:
            atomic_init(&sync->rwlock.readers, 0);
            atomic_flag_clear(&sync->rwlock.writer);
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

    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            if (!sync->use_lockfree) {
                int ret = pthread_mutex_trylock(&sync->mutex);
                if (ret == 0) {
                    return PPDB_OK;
                } else if (ret == EBUSY) {
                    return PPDB_ERR_BUSY;
                } else {
                    return PPDB_ERR_LOCK_FAILED;
                }
            } else {
                bool was_locked = atomic_flag_test_and_set(&sync->spinlock);
                return was_locked ? PPDB_ERR_BUSY : PPDB_OK;
            }
            break;

        case PPDB_SYNC_SPINLOCK:
            if (atomic_flag_test_and_set(&sync->spinlock)) {
                return PPDB_ERR_BUSY;
            }
            return PPDB_OK;

        case PPDB_SYNC_RWLOCK:
            // 检查是否有读者或写者
            if (atomic_load(&sync->rwlock.readers) > 0 || 
                atomic_flag_test_and_set(&sync->rwlock.writer)) {
                return PPDB_ERR_BUSY;
            }
            return PPDB_OK;

        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }

    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            if (!sync->use_lockfree) {
                if (pthread_mutex_unlock(&sync->mutex) != 0) {
                    return PPDB_ERR_UNLOCK_FAILED;
                }
            } else {
                atomic_flag_clear(&sync->spinlock);
            }
            break;

        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;

        case PPDB_SYNC_RWLOCK:
            atomic_flag_clear(&sync->rwlock.writer);
            break;

        default:
            return PPDB_ERR_INVALID_TYPE;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }

    if (sync->type != PPDB_SYNC_RWLOCK) {
        return PPDB_ERR_INVALID_TYPE;
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

    // 增加读者计数
    atomic_fetch_add(&sync->rwlock.readers, 1);

    return PPDB_OK;
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

    if (sync->type != PPDB_SYNC_RWLOCK) {
        return PPDB_ERR_INVALID_TYPE;
    }

    // 首先获取写者标志
    uint32_t spin_count = 0;
    while (atomic_flag_test_and_set(&sync->rwlock.writer)) {
        if (spin_count++ > sync->spin_count) {
            usleep(sync->backoff_us);
            spin_count = 0;
        } else {
            __asm__ volatile("pause");
        }
    }

    // 等待所有读者完成
    spin_count = 0;
    while (atomic_load(&sync->rwlock.readers) > 0) {
        if (spin_count++ > sync->spin_count) {
            usleep(sync->backoff_us);
            spin_count = 0;
        } else {
            __asm__ volatile("pause");
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_PARAM;
    }

    if (sync->type != PPDB_SYNC_RWLOCK) {
        return PPDB_ERR_INVALID_TYPE;
    }

    // 释放写者标志
    atomic_flag_clear(&sync->rwlock.writer);

    return PPDB_OK;
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