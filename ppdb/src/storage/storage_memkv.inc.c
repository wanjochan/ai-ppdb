//-----------------------------------------------------------------------------
// 内部结构
//-----------------------------------------------------------------------------

#include <cosmopolitan.h>
#include "internal/storage.h"
#include "internal/engine.h"

/*
 * TODO(improvement): LRU功能需要重构
 * 1. 使用engine层的事务和表操作接口
 * 2. 将LRU信息存储在单独的engine表中
 * 3. 实现原子的更新操作
 */

// memkv实例结构
typedef struct {
    ppdb_engine_t* engine;             // 引擎实例
    ppdb_engine_table_t* table;        // 数据表
    ppdb_engine_mutex_t* global_lock;  // 全局锁
    struct {
        uint64_t total_items;          // 总项数
        uint64_t total_memory;         // 总内存使用
        uint64_t hits;                 // 命中次数
        uint64_t misses;               // 未命中次数
        uint64_t evictions;            // 驱逐次数
    } stats;
} storage_memkv_t; 

//-----------------------------------------------------------------------------
// 存储操作实现
//-----------------------------------------------------------------------------

static ppdb_error_t memkv_init(void** ctx, const ppdb_options_t* options) {
    // 分配并初始化内存
    storage_memkv_t* kv = ppdb_engine_malloc(sizeof(storage_memkv_t));
    if (!kv) {
        return PPDB_ERR_MEMORY;
    }
    ppdb_engine_memset(kv, 0, sizeof(storage_memkv_t));
    
    // 初始化引擎
    ppdb_engine_config_t engine_config = {
        .memory_limit = options->cache_size,
        .thread_pool_size = 4,
        .thread_safe = true
    };
    
    ppdb_error_t err = ppdb_engine_init(&kv->engine, &engine_config);
    if (err != PPDB_OK) {
        ppdb_engine_free(kv);
        return err;
    }
    
    // 创建全局锁
    err = ppdb_engine_mutex_create(&kv->global_lock);
    if (err != PPDB_OK) {
        ppdb_engine_destroy(kv->engine);
        ppdb_engine_free(kv);
        return err;
    }
    
    // 创建引擎表
    err = ppdb_engine_table_create(kv->engine, "memkv", &kv->table);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_destroy(kv->global_lock);
        ppdb_engine_destroy(kv->engine);
        ppdb_engine_free(kv);
        return err;
    }
    
    *ctx = kv;
    return PPDB_OK;
}

static void memkv_destroy(void* ctx) {
    storage_memkv_t* kv = (storage_memkv_t*)ctx;
    if (!kv) {
        return;
    }
    
    ppdb_engine_table_close(kv->table);
    ppdb_engine_mutex_destroy(kv->global_lock);
    ppdb_engine_destroy(kv->engine);
    ppdb_engine_free(kv);
}

static ppdb_error_t memkv_get(void* ctx, const ppdb_data_t* key, ppdb_data_t* value) {
    storage_memkv_t* kv = (storage_memkv_t*)ctx;
    if (!kv || !key || !value) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_error_t err = ppdb_engine_mutex_lock(kv->global_lock);
    if (err != PPDB_OK) {
        return err;
    }

    // 开始事务
    ppdb_engine_tx_t* tx = NULL;
    err = ppdb_engine_begin_tx(kv->engine, &tx);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(kv->global_lock);
        return err;
    }
    
    err = ppdb_engine_get(tx, kv->table, key, value);
    if (err == PPDB_OK) {
        kv->stats.hits++;
        // TODO: 实现新的LRU更新机制
    } else if (err == PPDB_ERR_NOT_FOUND) {
        kv->stats.misses++;
    }
    
    // 提交事务
    ppdb_error_t commit_err = ppdb_engine_commit_tx(tx);
    if (commit_err != PPDB_OK) {
        ppdb_engine_rollback_tx(tx);
        ppdb_engine_mutex_unlock(kv->global_lock);
        return commit_err;
    }
    
    ppdb_engine_mutex_unlock(kv->global_lock);
    return err;
}

static ppdb_error_t memkv_put(void* ctx, const ppdb_data_t* key, const ppdb_data_t* value) {
    storage_memkv_t* kv = (storage_memkv_t*)ctx;
    if (!kv || !key || !value) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_error_t err = ppdb_engine_mutex_lock(kv->global_lock);
    if (err != PPDB_OK) {
        return err;
    }
    
    // TODO: 实现新的内存限制检查机制
    size_t required_memory = key->size + value->size;
    if (kv->stats.total_memory + required_memory > kv->engine->config.memory_limit) {
        // TODO: 实现新的数据淘汰机制
        ppdb_engine_mutex_unlock(kv->global_lock);
        return PPDB_ERR_NO_MEMORY;
    }

    // 开始事务
    ppdb_engine_tx_t* tx = NULL;
    err = ppdb_engine_begin_tx(kv->engine, &tx);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(kv->global_lock);
        return err;
    }
    
    // 存储数据
    err = ppdb_engine_put(tx, kv->table, key, value);
    if (err == PPDB_OK) {
        kv->stats.total_items++;
        kv->stats.total_memory += required_memory;
        
        // 提交事务
        err = ppdb_engine_commit_tx(tx);
        if (err != PPDB_OK) {
            ppdb_engine_rollback_tx(tx);
            ppdb_engine_mutex_unlock(kv->global_lock);
            return err;
        }
    } else {
        ppdb_engine_rollback_tx(tx);
    }
    
    ppdb_engine_mutex_unlock(kv->global_lock);
    return err;
}

static ppdb_error_t memkv_delete(void* ctx, const ppdb_data_t* key) {
    storage_memkv_t* kv = (storage_memkv_t*)ctx;
    if (!kv || !key) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_error_t err = ppdb_engine_mutex_lock(kv->global_lock);
    if (err != PPDB_OK) {
        return err;
    }

    // 开始事务
    ppdb_engine_tx_t* tx = NULL;
    err = ppdb_engine_begin_tx(kv->engine, &tx);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(kv->global_lock);
        return err;
    }
    
    ppdb_data_t old_value;
    err = ppdb_engine_get(tx, kv->table, key, &old_value);
    if (err == PPDB_OK) {
        err = ppdb_engine_delete(tx, kv->table, key);
        if (err == PPDB_OK) {
            kv->stats.total_items--;
            kv->stats.total_memory -= (key->size + old_value.size);
            
            // 提交事务
            err = ppdb_engine_commit_tx(tx);
            if (err != PPDB_OK) {
                ppdb_engine_rollback_tx(tx);
                ppdb_engine_mutex_unlock(kv->global_lock);
                return err;
            }
        } else {
            ppdb_engine_rollback_tx(tx);
        }
    } else {
        ppdb_engine_rollback_tx(tx);
    }
    
    ppdb_engine_mutex_unlock(kv->global_lock);
    return err;
}

static ppdb_error_t memkv_clear(void* ctx) {
    storage_memkv_t* kv = (storage_memkv_t*)ctx;
    if (!kv) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_error_t err = ppdb_engine_mutex_lock(kv->global_lock);
    if (err != PPDB_OK) {
        return err;
    }

    // 开始事务
    ppdb_engine_tx_t* tx = NULL;
    err = ppdb_engine_begin_tx(kv->engine, &tx);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(kv->global_lock);
        return err;
    }
    
    // 清空表
    err = ppdb_engine_table_clear(tx, kv->table);
    if (err == PPDB_OK) {
        kv->stats.total_items = 0;
        kv->stats.total_memory = 0;
        
        // 提交事务
        err = ppdb_engine_commit_tx(tx);
        if (err != PPDB_OK) {
            ppdb_engine_rollback_tx(tx);
            ppdb_engine_mutex_unlock(kv->global_lock);
            return err;
        }
    } else {
        ppdb_engine_rollback_tx(tx);
    }
    
    ppdb_engine_mutex_unlock(kv->global_lock);
    return err;
}

static ppdb_error_t memkv_get_stats(void* ctx, char* buffer, size_t size) {
    storage_memkv_t* kv = (storage_memkv_t*)ctx;
    if (!kv || !buffer || size == 0) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_error_t err = ppdb_engine_mutex_lock(kv->global_lock);
    if (err != PPDB_OK) {
        return err;
    }
    
    // 使用engine层的字符串操作函数
    int written = ppdb_engine_snprintf(buffer, size,
        "STAT curr_items %lu\r\n"
        "STAT bytes %lu\r\n"
        "STAT get_hits %lu\r\n"
        "STAT get_misses %lu\r\n"
        "STAT evictions %lu\r\n"
        "END\r\n",
        kv->stats.total_items,
        kv->stats.total_memory,
        kv->stats.hits,
        kv->stats.misses,
        kv->stats.evictions
    );
    
    ppdb_engine_mutex_unlock(kv->global_lock);
    
    if (written >= size) {
        return PPDB_ERR_BUFFER_FULL;
    }
    
    return PPDB_OK;
}

// 存储操作表
const storage_ops_t storage_memkv_ops = {
    .init = memkv_init,
    .destroy = memkv_destroy,
    .get = memkv_get,
    .put = memkv_put,
    .delete = memkv_delete,
    .clear = memkv_clear,
    .get_stats = memkv_get_stats
}; 