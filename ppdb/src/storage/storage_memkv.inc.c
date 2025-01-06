//-----------------------------------------------------------------------------
// 内部结构
//-----------------------------------------------------------------------------

// memkv实例结构
typedef struct {
    ppdb_base_t* base;                // 基础设施
    ppdb_base_mutex_t* global_lock;   // 全局锁
    ppdb_base_skiplist_t* data;       // 数据存储
    struct {
        uint64_t total_items;         // 总项数
        uint64_t total_memory;        // 总内存使用
        uint64_t hits;                // 命中次数
        uint64_t misses;              // 未命中次数
        uint64_t evictions;           // 驱逐次数
    } stats;
} storage_memkv_t;

//-----------------------------------------------------------------------------
// 存储操作实现
//-----------------------------------------------------------------------------

static ppdb_error_t memkv_init(void** ctx, const ppdb_options_t* options) {
    storage_memkv_t* kv = calloc(1, sizeof(storage_memkv_t));
    if (!kv) {
        return PPDB_ERR_MEMORY;
    }
    
    // 初始化基础设施
    ppdb_base_config_t base_config = {
        .memory_limit = options->cache_size,
        .thread_pool_size = 4,
        .thread_safe = true
    };
    
    ppdb_error_t err = ppdb_base_init(&kv->base, &base_config);
    if (err != PPDB_OK) {
        free(kv);
        return err;
    }
    
    // 创建全局锁
    err = ppdb_base_mutex_create(&kv->global_lock);
    if (err != PPDB_OK) {
        ppdb_base_destroy(kv->base);
        free(kv);
        return err;
    }
    
    // 创建跳表
    err = ppdb_base_skiplist_create(&kv->data);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(kv->global_lock);
        ppdb_base_destroy(kv->base);
        free(kv);
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
    
    ppdb_base_skiplist_destroy(kv->data);
    ppdb_base_mutex_destroy(kv->global_lock);
    ppdb_base_destroy(kv->base);
    free(kv);
}

static ppdb_error_t memkv_get(void* ctx, const ppdb_data_t* key, ppdb_data_t* value) {
    storage_memkv_t* kv = (storage_memkv_t*)ctx;
    if (!kv || !key || !value) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_error_t err = ppdb_base_mutex_lock(kv->global_lock);
    if (err != PPDB_OK) {
        return err;
    }
    
    err = ppdb_base_skiplist_get(kv->data, key, value);
    if (err == PPDB_OK) {
        kv->stats.hits++;
    } else if (err == PPDB_ERR_NOT_FOUND) {
        kv->stats.misses++;
    }
    
    ppdb_base_mutex_unlock(kv->global_lock);
    return err;
}

static ppdb_error_t memkv_put(void* ctx, const ppdb_data_t* key, const ppdb_data_t* value) {
    storage_memkv_t* kv = (storage_memkv_t*)ctx;
    if (!kv || !key || !value) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_error_t err = ppdb_base_mutex_lock(kv->global_lock);
    if (err != PPDB_OK) {
        return err;
    }
    
    err = ppdb_base_skiplist_put(kv->data, key, value);
    if (err == PPDB_OK) {
        kv->stats.total_items++;
        kv->stats.total_memory += key->size + value->size;
    }
    
    ppdb_base_mutex_unlock(kv->global_lock);
    return err;
}

static ppdb_error_t memkv_delete(void* ctx, const ppdb_data_t* key) {
    storage_memkv_t* kv = (storage_memkv_t*)ctx;
    if (!kv || !key) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_error_t err = ppdb_base_mutex_lock(kv->global_lock);
    if (err != PPDB_OK) {
        return err;
    }
    
    ppdb_data_t old_value;
    err = ppdb_base_skiplist_delete(kv->data, key, &old_value);
    if (err == PPDB_OK) {
        kv->stats.total_items--;
        kv->stats.total_memory -= key->size + old_value.size;
    }
    
    ppdb_base_mutex_unlock(kv->global_lock);
    return err;
}

static ppdb_error_t memkv_clear(void* ctx) {
    storage_memkv_t* kv = (storage_memkv_t*)ctx;
    if (!kv) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_error_t err = ppdb_base_mutex_lock(kv->global_lock);
    if (err != PPDB_OK) {
        return err;
    }
    
    err = ppdb_base_skiplist_clear(kv->data);
    if (err == PPDB_OK) {
        memset(&kv->stats, 0, sizeof(kv->stats));
    }
    
    ppdb_base_mutex_unlock(kv->global_lock);
    return err;
}

static ppdb_error_t memkv_get_stats(void* ctx, char* buffer, size_t size) {
    storage_memkv_t* kv = (storage_memkv_t*)ctx;
    if (!kv || !buffer || size == 0) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_error_t err = ppdb_base_mutex_lock(kv->global_lock);
    if (err != PPDB_OK) {
        return err;
    }
    
    int len = snprintf(buffer, size,
                      "MemKV Stats:\n"
                      "  Total Items: %lu\n"
                      "  Total Memory: %lu bytes\n"
                      "  Cache Hits: %lu\n"
                      "  Cache Misses: %lu\n"
                      "  Evictions: %lu\n",
                      kv->stats.total_items,
                      kv->stats.total_memory,
                      kv->stats.hits,
                      kv->stats.misses,
                      kv->stats.evictions);
    
    ppdb_base_mutex_unlock(kv->global_lock);
    
    if (len >= size) {
        return PPDB_ERR_BUFFER_TOO_SMALL;
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