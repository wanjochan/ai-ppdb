#include <cosmopolitan.h>
#include "internal/sharded_memtable.h"
#include "internal/sync.h"
#include "ppdb/logger.h"

// Calculate key hash
static uint32_t hash_key(const uint8_t* key, size_t key_len) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < key_len; i++) {
        hash = ((hash << 5) + hash) + key[i];
    }
    return hash;
}

// Calculate shard index based on key
static uint32_t get_shard_index(const sharded_memtable_t* table,
                              const uint8_t* key, size_t key_len) {
    uint32_t hash = hash_key(key, key_len);
    return hash & ((1 << table->config.shard_bits) - 1);
}

// 分片内存表结构
struct sharded_memtable_t {
    shard_config_t config;          // 分片配置
    ppdb_skiplist_t** shards;       // 分片数组
    ppdb_sync_t sync;               // 全局同步
    ppdb_sync_t* shard_syncs;       // 分片同步数组
    atomic_size_t total_size;       // 总元素数
    atomic_bool is_immutable;       // 是否不可变
};

// 创建分片内存表
sharded_memtable_t* sharded_memtable_create(const shard_config_t* config) {
    if (!config || config->shard_bits > 16) return NULL;

    sharded_memtable_t* table = aligned_alloc(64, sizeof(sharded_memtable_t));
    if (!table) {
        ppdb_log_error("Failed to allocate sharded memtable");
        return NULL;
    }

    // 复制配置
    memcpy(&table->config, config, sizeof(shard_config_t));
    uint32_t shard_count = 1 << config->shard_bits;

    // 分配分片数组
    table->shards = aligned_alloc(64, shard_count * sizeof(ppdb_skiplist_t*));
    if (!table->shards) {
        free(table);
        ppdb_log_error("Failed to allocate shards array");
        return NULL;
    }

    // 初始化同步原语
    ppdb_sync_config_t sync_config = PPDB_SYNC_DEFAULT_CONFIG;
    ppdb_sync_init(&table->sync, &sync_config);

    // 初始化分片同步
    table->shard_syncs = aligned_alloc(64, shard_count * sizeof(ppdb_sync_t));
    if (!table->shard_syncs) {
        ppdb_sync_destroy(&table->sync);
        free(table->shards);
        free(table);
        ppdb_log_error("Failed to allocate shard syncs array");
        return NULL;
    }

    // 初始化每个分片
    for (uint32_t i = 0; i < shard_count; i++) {
        ppdb_sync_init(&table->shard_syncs[i], &sync_config);

        ppdb_skiplist_config_t sl_config = {
            .max_level = 12,
            .sync_config = sync_config,
            .enable_hint = true
        };
        table->shards[i] = ppdb_skiplist_create(&sl_config);
        
        if (!table->shards[i]) {
            for (uint32_t j = 0; j < i; j++) {
                ppdb_skiplist_destroy(table->shards[j]);
                ppdb_sync_destroy(&table->shard_syncs[j]);
            }
            ppdb_sync_destroy(&table->sync);
            free(table->shard_syncs);
            free(table->shards);
            free(table);
            ppdb_log_error("Failed to create skiplist for shard %u", i);
            return NULL;
        }
    }

    atomic_init(&table->total_size, 0);
    atomic_init(&table->is_immutable, false);

    return table;
}

// 销毁分片内存表
void sharded_memtable_destroy(sharded_memtable_t* table) {
    if (!table) return;

    uint32_t shard_count = 1 << table->config.shard_bits;

    // 销毁所有分片
    for (uint32_t i = 0; i < shard_count; i++) {
        if (table->shards[i]) {
            ppdb_skiplist_destroy(table->shards[i]);
        }
        ppdb_sync_destroy(&table->shard_syncs[i]);
    }

    ppdb_sync_destroy(&table->sync);
    free(table->shard_syncs);
    free(table->shards);
    free(table);
}

// 插入键值对
int sharded_memtable_put(sharded_memtable_t* table, const uint8_t* key, size_t key_len,
                        const uint8_t* value, size_t value_len) {
    if (!table || !key || !value) return PPDB_ERR_INVALID_ARG;

    // 检查是否不可变
    if (atomic_load(&table->is_immutable)) {
        return PPDB_ERR_IMMUTABLE;
    }

    // 获取分片索引
    uint32_t shard_idx = get_shard_index(table, key, key_len);
    ppdb_skiplist_t* shard = table->shards[shard_idx];

    // 尝试插入
    int result = ppdb_skiplist_insert(shard, key, key_len, value, value_len);
    
    if (result == PPDB_OK) {
        size_t entry_size = key_len + value_len + sizeof(ppdb_skiplist_node_t);
        atomic_fetch_add(&table->total_size, entry_size);
        return PPDB_OK;
    }

    return PPDB_ERR_INTERNAL;
}

// 删除键值对
int sharded_memtable_delete(sharded_memtable_t* table, const uint8_t* key, size_t key_len) {
    if (!table || !key) return PPDB_ERR_INVALID_ARG;

    // 检查是否不可变
    if (atomic_load(&table->is_immutable)) {
        return PPDB_ERR_IMMUTABLE;
    }

    // 获取分片索引
    uint32_t shard_idx = get_shard_index(table, key, key_len);
    ppdb_skiplist_t* shard = table->shards[shard_idx];

    int result = ppdb_skiplist_remove(shard, key, key_len);
    
    if (result == PPDB_OK) {
        return PPDB_OK;
    } else if (result == PPDB_NOT_FOUND) {
        return PPDB_ERR_NOT_FOUND;
    }

    return PPDB_ERR_INTERNAL;
}

// 查找键值对
int sharded_memtable_get(sharded_memtable_t* table, const uint8_t* key, size_t key_len,
                        uint8_t** value, size_t* value_len) {
    if (!table || !key || !value || !value_len) return PPDB_ERR_INVALID_ARG;

    // 获取分片索引
    uint32_t shard_idx = get_shard_index(table, key, key_len);
    ppdb_skiplist_t* shard = table->shards[shard_idx];

    int result = ppdb_skiplist_find(shard, key, key_len, (void**)value, value_len);
    
    if (result == PPDB_OK) {
        return PPDB_OK;
    } else if (result == PPDB_NOT_FOUND) {
        return PPDB_ERR_NOT_FOUND;
    }

    return PPDB_ERR_INTERNAL;
}

// 获取总元素数
size_t sharded_memtable_size(sharded_memtable_t* table) {
    if (!table) return 0;
    return atomic_load(&table->total_size);
}

// 获取指定分片的元素数
size_t sharded_memtable_shard_size(sharded_memtable_t* table, uint32_t shard_index) {
    if (!table || shard_index >= (1u << table->config.shard_bits)) return 0;
    return ppdb_skiplist_size(table->shards[shard_index]);
}

// 清空分片内存表
void sharded_memtable_clear(sharded_memtable_t* table) {
    if (!table) return;

    uint32_t shard_count = 1 << table->config.shard_bits;

    // 销毁并重新创建所有分片
    ppdb_sync_config_t sync_config = PPDB_SYNC_DEFAULT_CONFIG;
    for (uint32_t i = 0; i < shard_count; i++) {
        ppdb_skiplist_destroy(table->shards[i]);
        
        ppdb_skiplist_config_t sl_config = {
            .max_level = 12,
            .sync_config = sync_config,
            .enable_hint = true
        };
        table->shards[i] = ppdb_skiplist_create(&sl_config);
        
        if (!table->shards[i]) {
            ppdb_log_error("Failed to recreate skiplist for shard %u", i);
            continue;
        }
    }

    atomic_store(&table->total_size, 0);
}

// 遍历分片内存表
void sharded_memtable_foreach(sharded_memtable_t* table, memtable_visitor_t visitor, void* ctx) {
    if (!table || !visitor) return;

    uint32_t shard_count = 1 << table->config.shard_bits;

    // 遍历每个分片
    for (uint32_t i = 0; i < shard_count; i++) {
        ppdb_skiplist_t* shard = table->shards[i];
        ppdb_skiplist_iterator_t* iter = ppdb_skiplist_iterator_create(shard);
        
        if (!iter) continue;

        while (ppdb_skiplist_iterator_next(iter) == PPDB_OK) {
            void* key;
            void* value;
            size_t key_len, value_len;
            
            if (ppdb_skiplist_iterator_get(iter, &key, &key_len, &value, &value_len) == PPDB_OK) {
                visitor(key, key_len, value, value_len, ctx);
                free(key);
                free(value);
            }
        }

        ppdb_skiplist_iterator_destroy(iter);
    }
}

// 设置为不可变
void sharded_memtable_set_immutable(sharded_memtable_t* table) {
    if (!table) return;
    atomic_store(&table->is_immutable, true);
}

// 检查是否不可变
bool sharded_memtable_is_immutable(sharded_memtable_t* table) {
    if (!table) return true;
    return atomic_load(&table->is_immutable);
}
