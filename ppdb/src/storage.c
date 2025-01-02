#include "ppdb/storage.h"
#include "cosmopolitan.h"

// Skiplist 实现
static ppdb_error_t skiplist_init(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 分配头节点
    base->storage.head = calloc(1, sizeof(ppdb_node_t));
    if (!base->storage.head) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化内存池
    size_t pool_size = config->initial_size > 0 ? config->initial_size : 1024 * 1024;  // 默认1MB
    base->storage.pool = malloc(pool_size);
    if (!base->storage.pool) {
        free(base->storage.head);
        base->storage.head = NULL;
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化统计信息
    memset(&base->storage.metrics, 0, sizeof(ppdb_metrics_t));

    return PPDB_OK;
}

static ppdb_error_t skiplist_destroy(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 释放头节点和内存池
    if (base->storage.head) {
        free(base->storage.head);
        base->storage.head = NULL;
    }
    if (base->storage.pool) {
        free(base->storage.pool);
        base->storage.pool = NULL;
    }

    return PPDB_OK;
}

static ppdb_error_t skiplist_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value || !base->storage.head) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_node_t* current = base->storage.head;
    ppdb_node_t* next;

    // 从最高层开始查找
    for (int level = 31; level >= 0; level--) {
        next = (ppdb_node_t*)((uintptr_t)current->ptr & ~((1ULL << level) - 1));
        while (next && memcmp(&next->data, key, sizeof(ppdb_key_t)) < 0) {
            current = next;
            next = (ppdb_node_t*)((uintptr_t)current->ptr & ~((1ULL << level) - 1));
        }
    }

    // 检查是否找到
    next = (ppdb_node_t*)((uintptr_t)current->ptr & ~0ULL);
    if (next && memcmp(&next->data, key, sizeof(ppdb_key_t)) == 0) {
        *value = *(ppdb_value_t*)next->extra;
        atomic_fetch_add(&base->storage.metrics.get_hits, 1);
        return PPDB_OK;
    }

    atomic_fetch_add(&base->storage.metrics.get_miss_count, 1);
    return PPDB_ERR_NOT_FOUND;
}

static ppdb_error_t skiplist_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value || !base->storage.head || !base->storage.pool) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 分配新节点
    ppdb_node_t* new_node = base->storage.pool;
    base->storage.pool = (char*)base->storage.pool + sizeof(ppdb_node_t);

    // 初始化节点
    memcpy(&new_node->data, key->data, sizeof(uint64_t));  // 假设键是 8 字节
    new_node->extra = base->storage.pool;
    base->storage.pool = (char*)base->storage.pool + sizeof(ppdb_value_t);
    *(ppdb_value_t*)new_node->extra = *value;

    // 随机生成层数
    int level = 0;
    uint32_t random = 0;
    while ((random & 1) == 0 && level < 31) {
        level++;
        random = (random >> 1) | (random << 31);  // 简单的随机数生成
    }

    // 从最高层开始插入
    ppdb_node_t* current = base->storage.head;
    for (int i = 31; i >= 0; i--) {
        ppdb_node_t* next = (ppdb_node_t*)((uintptr_t)current->ptr & ~((1ULL << i) - 1));
        while (next && memcmp(&next->data, key->data, sizeof(uint64_t)) < 0) {
            current = next;
            next = (ppdb_node_t*)((uintptr_t)next->ptr & ~((1ULL << i) - 1));
        }
        if (i <= level) {
            new_node->ptr = (void*)((uintptr_t)next | ((1ULL << i) - 1));
            current->ptr = (void*)((uintptr_t)new_node | ((1ULL << i) - 1));
        }
    }

    // 更新统计信息
    atomic_fetch_add(&base->storage.metrics.put_count, 1);
    atomic_fetch_add(&base->storage.metrics.total_bytes, key->size + value->size);
    atomic_fetch_add(&base->storage.metrics.total_keys, 1);
    atomic_fetch_add(&base->storage.metrics.total_values, 1);

    return PPDB_OK;
}

// Memtable 实现
static ppdb_error_t memtable_init(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }
    // 使用 skiplist 作为底层存储
    return skiplist_init(base, config);
}

// Sharded 实现
static ppdb_error_t sharded_init(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 创建分片数组
    uint32_t shard_count = 16;  // 默认16个分片
    base->array.count = shard_count;
    base->array.ptrs = calloc(shard_count, sizeof(void*));
    if (!base->array.ptrs) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化每个分片
    ppdb_storage_config_t shard_config = *config;
    shard_config.initial_size /= shard_count;
    for (uint32_t i = 0; i < shard_count; i++) {
        ppdb_base_t* shard = malloc(sizeof(ppdb_base_t));
        if (!shard) {
            // 清理已创建的分片
            for (uint32_t j = 0; j < i; j++) {
                skiplist_destroy(base->array.ptrs[j]);
                free(base->array.ptrs[j]);
            }
            free(base->array.ptrs);
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        if (skiplist_init(shard, &shard_config) != PPDB_OK) {
            free(shard);
            for (uint32_t j = 0; j < i; j++) {
                skiplist_destroy(base->array.ptrs[j]);
                free(base->array.ptrs[j]);
            }
            free(base->array.ptrs);
            return PPDB_ERR_INTERNAL;
        }
        base->array.ptrs[i] = shard;
    }

    return PPDB_OK;
}

// KVStore 实现
static ppdb_error_t kvstore_init(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }
    // 使用 sharded memtable 作为存储引擎
    return sharded_init(base, config);
}

// 通用操作实现
ppdb_error_t ppdb_storage_sync(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }
    // TODO: 实现存储同步
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_flush(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }
    // TODO: 实现存储刷新
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_compact(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }
    // TODO: 实现存储压缩
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get_stats(ppdb_base_t* base, ppdb_storage_stats_t* stats) {
    if (!base || !stats) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 复制基础统计信息
    stats->base_metrics = base->storage.metrics;
    
    // TODO: 计算其他统计信息
    return PPDB_OK;
}

// 导出的创建函数
ppdb_error_t ppdb_skiplist_create(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }
    base->header.type = PPDB_TYPE_SKIPLIST;
    return skiplist_init(base, config);
}

ppdb_error_t ppdb_memtable_create(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }
    base->header.type = PPDB_TYPE_MEMTABLE;
    return memtable_init(base, config);
}

ppdb_error_t ppdb_sharded_create(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }
    base->header.type = PPDB_TYPE_SHARDED;
    return sharded_init(base, config);
}

ppdb_error_t ppdb_kvstore_create(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }
    base->header.type = PPDB_TYPE_KVSTORE;
    return kvstore_init(base, config);
}
