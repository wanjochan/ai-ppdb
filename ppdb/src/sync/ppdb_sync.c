#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "ppdb/ppdb_sync.h"
#include "sync/internal/internal_sync.h"

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
static inline void ppdb_sync_pause(void) {
    sync_pause();
}

static inline void ppdb_sync_backoff(uint32_t backoff_us) {
    sync_backoff(backoff_us);
}

static inline bool ppdb_sync_should_yield(uint32_t spin_count, uint32_t current_spins) {
    return sync_should_yield(spin_count, current_spins);
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

    // 初始化基础同步结构
    sync_config_t base_config = {
        .type = (sync_type_t)config->type,
        .spin_count = config->spin_count,
        .backoff_us = config->backoff_us,
        .max_readers = config->max_readers
    };
    
    ppdb_error_t err = sync_init(&sync->base, &base_config);
    if (err != PPDB_OK) {
        return err;
    }

    // 初始化扩展字段
    sync->type = config->type;
    sync->use_lockfree = config->use_lockfree;
    sync->enable_fairness = config->enable_fairness;
    sync->enable_ref_count = config->enable_ref_count;
    sync->spin_count = config->spin_count;
    sync->backoff_us = config->backoff_us;
    sync->max_readers = config->max_readers;

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 销毁基础同步结构
    return sync_destroy(&sync->base);
}

// 锁操作
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }
    return sync_try_lock(&sync->base);
}

ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }
    return sync_unlock(&sync->base);
}

// 读写锁操作
ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }
    return sync_read_lock(&sync->base);
}

ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }
    return sync_read_unlock(&sync->base);
}

ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }
    return sync_write_lock(&sync->base);
}

ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }
    return sync_write_unlock(&sync->base);
}

// 共享读锁操作
ppdb_error_t ppdb_sync_read_lock_shared(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }
    return sync_read_lock_shared(&sync->base);
}

ppdb_error_t ppdb_sync_read_unlock_shared(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }
    return sync_read_unlock_shared(&sync->base);
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