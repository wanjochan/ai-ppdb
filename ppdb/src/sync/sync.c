#include <cosmopolitan.h>
#include "ppdb/sync.h"
#include "ppdb/ppdb_logger.h"

// 无锁操作的参数结构
struct put_args {
    void* key;
    size_t key_len;
    void* value;
    size_t value_len;
};

struct get_args {
    void* key;
    size_t key_len;
    void** value;
    size_t* value_len;
};

struct delete_args {
    void* key;
    size_t key_len;
};

// 无锁操作的重试逻辑
static ppdb_error_t ppdb_sync_retry(ppdb_sync_t* sync, ppdb_sync_config_t* config,
                                   ppdb_error_t (*op)(ppdb_sync_t*, void*),
                                   void* arg) {
    if (!config->use_lockfree) {
        return op(sync, arg);
    }

    uint32_t retries = 0;
    ppdb_error_t err;
    do {
        err = op(sync, arg);
        if (err == PPDB_OK) {
            return PPDB_OK;
        }
        if (err != PPDB_ERR_RETRY) {
            return err;
        }
        usleep(config->retry_delay_us);
        retries++;
    } while (retries < config->retry_count);

    return PPDB_ERR_TIMEOUT;
}

// 无锁操作的单次尝试函数
static ppdb_error_t ppdb_sync_lockfree_put_once(ppdb_sync_t* sync, void* arg) {
    struct put_args* args = (struct put_args*)arg;
    
    // 使用CAS操作尝试更新值
    if (!sync || !args->key || !args->value) {
        return PPDB_ERR_INVALID_ARG;
    }

    // TODO(分片优化): 目前计算了哈希值但未使用，后续实现分片锁时会用于选择具体的锁
    uint32_t hash = ppdb_sync_hash(args->key, args->key_len);
    
    // 尝试获取对应分片的锁
    if (!ppdb_sync_try_lock(sync)) {
        return PPDB_ERR_RETRY;
    }

    // TODO: 在这里实现实际的数据更新逻辑
    // 目前先返回成功，后续实现具体的数据结构操作
    ppdb_sync_unlock(sync);
    return PPDB_OK;
}

static ppdb_error_t ppdb_sync_lockfree_get_once(ppdb_sync_t* sync, void* arg) {
    struct get_args* args = (struct get_args*)arg;
    
    if (!sync || !args->key || !args->value || !args->value_len) {
        return PPDB_ERR_INVALID_ARG;
    }

    // TODO(分片优化): 目前计算了哈希值但未使用，后续实现分片锁时会用于选择具体的锁
    uint32_t hash = ppdb_sync_hash(args->key, args->key_len);
    
    // 尝试获取对应分片的读锁
    if (!ppdb_sync_try_lock(sync)) {
        return PPDB_ERR_RETRY;
    }

    // TODO: 在这里实现实际的数据读取逻辑
    // 目前先返回成功，后续实现具体的数据结构操作
    ppdb_sync_unlock(sync);
    return PPDB_OK;
}

static ppdb_error_t ppdb_sync_lockfree_delete_once(ppdb_sync_t* sync, void* arg) {
    struct delete_args* args = (struct delete_args*)arg;
    
    if (!sync || !args->key) {
        return PPDB_ERR_INVALID_ARG;
    }

    // TODO(分片优化): 目前计算了哈希值但未使用，后续实现分片锁时会用于选择具体的锁
    uint32_t hash = ppdb_sync_hash(args->key, args->key_len);
    
    // 尝试获取对应分片的锁
    if (!ppdb_sync_try_lock(sync)) {
        return PPDB_ERR_RETRY;
    }

    // TODO: 在这里实现实际的数据删除逻辑
    // 目前先返回成功，后续实现具体的数据结构操作
    ppdb_sync_unlock(sync);
    return PPDB_OK;
}

// 无锁操作的接口函数
ppdb_error_t ppdb_sync_lockfree_put(ppdb_sync_t* sync, void* key, size_t key_len,
                                   void* value, size_t value_len,
                                   ppdb_sync_config_t* config) {
    struct put_args args = {
        .key = key,
        .key_len = key_len,
        .value = value,
        .value_len = value_len
    };
    return ppdb_sync_retry(sync, config, ppdb_sync_lockfree_put_once, &args);
}

ppdb_error_t ppdb_sync_lockfree_get(ppdb_sync_t* sync, void* key, size_t key_len,
                                   void** value, size_t* value_len,
                                   ppdb_sync_config_t* config) {
    struct get_args args = {
        .key = key,
        .key_len = key_len,
        .value = value,
        .value_len = value_len
    };
    return ppdb_sync_retry(sync, config, ppdb_sync_lockfree_get_once, &args);
}

ppdb_error_t ppdb_sync_lockfree_delete(ppdb_sync_t* sync, void* key, size_t key_len,
                                      ppdb_sync_config_t* config) {
    struct delete_args args = {
        .key = key,
        .key_len = key_len
    };
    return ppdb_sync_retry(sync, config, ppdb_sync_lockfree_delete_once, &args);
}

// FNV-1a哈希函数实现
uint32_t ppdb_sync_hash(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t hash = 2166136261u;  // FNV offset basis
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;  // FNV prime
    }
    return hash;
}

// 基本同步原语操作实现
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config) {
    if (!sync || !config) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 清零同步原语结构
    memset(sync, 0, sizeof(ppdb_sync_t));

    // 设置同步类型
    sync->type = config->type;

    // 根据类型初始化
    switch (config->type) {
        case PPDB_SYNC_MUTEX: {
            sync->mutex = 0;  // 初始化为未锁定状态
            break;
        }
        case PPDB_SYNC_SPINLOCK:
            sync->spinlock = 0;  // 初始化为未锁定状态
            break;
        case PPDB_SYNC_RWLOCK: {
            sync->rwlock.readers = 0;  // 初始化读者数量
            sync->rwlock.writer = 0;   // 初始化写者标志
            break;
        }
        default:
            return PPDB_ERR_INVALID_ARG;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 清零同步原语结构
    memset(sync, 0, sizeof(ppdb_sync_t));
    return PPDB_OK;
}

bool ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return false;
    }

    switch (sync->type) {
        case PPDB_SYNC_MUTEX: {
            int expected = 0;
            return __atomic_compare_exchange_n(&sync->mutex, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        }
        case PPDB_SYNC_SPINLOCK: {
            int expected = 0;
            return __atomic_compare_exchange_n(&sync->spinlock, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        }
        case PPDB_SYNC_RWLOCK: {
            // 尝试获取写锁
            if (__atomic_load_n(&sync->rwlock.readers, __ATOMIC_SEQ_CST) != 0) {
                return false;
            }
            int expected = 0;
            return __atomic_compare_exchange_n(&sync->rwlock.writer, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        }
        default:
            return false;
    }
}

ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    switch (sync->type) {
        case PPDB_SYNC_MUTEX: {
            while (true) {
                int expected = 0;
                if (__atomic_compare_exchange_n(&sync->mutex, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                    break;
                }
                usleep(1);  // 短暂休眠以减少CPU占用
            }
            break;
        }
        case PPDB_SYNC_SPINLOCK: {
            while (true) {
                int expected = 0;
                if (__atomic_compare_exchange_n(&sync->spinlock, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                    break;
                }
                usleep(1);  // 短暂休眠以减少CPU占用
            }
            break;
        }
        case PPDB_SYNC_RWLOCK: {
            // 等待所有读者完成
            while (__atomic_load_n(&sync->rwlock.readers, __ATOMIC_SEQ_CST) != 0) {
                usleep(1);
            }
            // 获取写锁
            while (true) {
                int expected = 0;
                if (__atomic_compare_exchange_n(&sync->rwlock.writer, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                    break;
                }
                usleep(1);
            }
            break;
        }
        default:
            return PPDB_ERR_INVALID_ARG;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            __atomic_store_n(&sync->mutex, 0, __ATOMIC_SEQ_CST);
            break;
        case PPDB_SYNC_SPINLOCK:
            __atomic_store_n(&sync->spinlock, 0, __ATOMIC_SEQ_CST);
            break;
        case PPDB_SYNC_RWLOCK:
            __atomic_store_n(&sync->rwlock.writer, 0, __ATOMIC_SEQ_CST);
            break;
        default:
            return PPDB_ERR_INVALID_ARG;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    if (sync->type != PPDB_SYNC_RWLOCK) return PPDB_ERR_INVALID_ARG;
    
    // 等待写者完成
    while (__atomic_load_n(&sync->rwlock.writer, __ATOMIC_SEQ_CST) != 0) {
        usleep(1);
    }
    
    // 增加读者计数
    __atomic_add_fetch(&sync->rwlock.readers, 1, __ATOMIC_SEQ_CST);
    
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    if (sync->type != PPDB_SYNC_RWLOCK) return PPDB_ERR_INVALID_ARG;
    
    // 减少读者计数
    __atomic_sub_fetch(&sync->rwlock.readers, 1, __ATOMIC_SEQ_CST);
    
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_file(const char* filename) {
    if (!filename) {
        return PPDB_ERR_INVALID_ARG;
    }

    int fd = open(filename, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        PPDB_LOG_ERROR("Failed to open file for sync: %s (errno: %d)", filename, errno);
        return PPDB_ERR_IO;
    }

    int ret = fsync(fd);
    close(fd);

    if (ret != 0) {
        PPDB_LOG_ERROR("Failed to sync file: %s (errno: %d)", filename, errno);
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_fd(int fd) {
    if (fd < 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    int ret = fsync(fd);
    if (ret != 0) {
        PPDB_LOG_ERROR("Failed to sync file descriptor: %d (errno: %d)", fd, errno);
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

ppdb_sync_t* ppdb_sync_create(void) {
    ppdb_sync_t* sync = malloc(sizeof(ppdb_sync_t));
    if (!sync) return NULL;

    // 初始化为默认配置
    ppdb_sync_config_t config = PPDB_SYNC_CONFIG_DEFAULT;

    if (ppdb_sync_init(sync, &config) != PPDB_OK) {
        free(sync);
        return NULL;
    }

    return sync;
} 