//-----------------------------------------------------------------------------
// 内部结构
//-----------------------------------------------------------------------------

// LRU 节点结构
typedef struct lru_node {
    struct lru_node* prev;
    struct lru_node* next;
    ppdb_data_t key;
    uint64_t last_access;
} lru_node_t;

// memkv实例结构
typedef struct {
    ppdb_base_t* base;                // 基础设施
    ppdb_base_mutex_t* global_lock;   // 全局锁
    ppdb_base_skiplist_t* data;       // 数据存储
    lru_node_t* lru_head;             // LRU 链表头
    lru_node_t* lru_tail;             // LRU 链表尾
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

// LRU 操作函数
static void lru_remove_node(storage_memkv_t* kv, lru_node_t* node) {
    if (!node->prev) {
        kv->lru_head = node->next;
    } else {
        node->prev->next = node->next;
    }
    
    if (!node->next) {
        kv->lru_tail = node->prev;
    } else {
        node->next->prev = node->prev;
    }
}

static void lru_add_to_front(storage_memkv_t* kv, lru_node_t* node) {
    node->prev = NULL;
    node->next = kv->lru_head;
    node->last_access = ppdb_base_get_current_time();
    
    if (kv->lru_head) {
        kv->lru_head->prev = node;
    }
    kv->lru_head = node;
    
    if (!kv->lru_tail) {
        kv->lru_tail = node;
    }
}

static void lru_update_access(storage_memkv_t* kv, const ppdb_data_t* key) {
    lru_node_t* node = ppdb_base_skiplist_get_user_data(kv->data, key);
    if (node) {
        lru_remove_node(kv, node);
        lru_add_to_front(kv, node);
    }
}

static ppdb_error_t lru_evict(storage_memkv_t* kv, size_t required_memory) {
    while (kv->stats.total_memory + required_memory > kv->base->config.memory_limit && kv->lru_tail) {
        lru_node_t* node = kv->lru_tail;
        ppdb_data_t value;
        
        // 获取要删除的键值对大小
        ppdb_error_t err = ppdb_base_skiplist_get(kv->data, &node->key, &value);
        if (err != PPDB_OK) {
            return err;
        }
        
        // 从跳表和LRU链表中删除
        err = ppdb_base_skiplist_delete(kv->data, &node->key);
        if (err != PPDB_OK) {
            return err;
        }
        
        lru_remove_node(kv, node);
        
        // 更新统计信息
        kv->stats.total_items--;
        kv->stats.total_memory -= (node->key.size + value.size);
        kv->stats.evictions++;
        
        // 释放资源
        free(node->key.data);
        free(node);
    }
    
    return PPDB_OK;
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
        lru_update_access(kv, key);
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
    
    // 检查内存限制并执行LRU驱逐
    size_t required_memory = key->size + value->size;
    if (kv->stats.total_memory + required_memory > kv->base->config.memory_limit) {
        err = lru_evict(kv, required_memory);
        if (err != PPDB_OK) {
            ppdb_base_mutex_unlock(kv->global_lock);
            return err;
        }
    }
    
    // 创建新的LRU节点
    lru_node_t* node = malloc(sizeof(lru_node_t));
    if (!node) {
        ppdb_base_mutex_unlock(kv->global_lock);
        return PPDB_ERR_MEMORY;
    }
    
    // 复制键
    node->key.data = malloc(key->size);
    if (!node->key.data) {
        free(node);
        ppdb_base_mutex_unlock(kv->global_lock);
        return PPDB_ERR_MEMORY;
    }
    memcpy(node->key.data, key->data, key->size);
    node->key.size = key->size;
    
    // 存储数据并关联LRU节点
    err = ppdb_base_skiplist_put_with_data(kv->data, key, value, node);
    if (err == PPDB_OK) {
        lru_add_to_front(kv, node);
        kv->stats.total_items++;
        kv->stats.total_memory += required_memory;
    } else {
        free(node->key.data);
        free(node);
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
    err = ppdb_base_skiplist_get(kv->data, key, &old_value);
    if (err == PPDB_OK) {
        err = ppdb_base_skiplist_delete(kv->data, key);
        if (err == PPDB_OK) {
            kv->stats.total_items--;
            kv->stats.total_memory -= (key->size + old_value.size);
        }
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
        kv->stats.total_items = 0;
        kv->stats.total_memory = 0;
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
    
    int written = snprintf(buffer, size,
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
    
    ppdb_base_mutex_unlock(kv->global_lock);
    
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