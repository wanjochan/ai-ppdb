#include <cosmopolitan.h>
#include "sharded_memtable.h"
#include "ppdb/logger.h"

// 计算键的哈希值
static uint32_t hash_key(const char* key, uint32_t key_len) {
    uint32_t hash = 5381;
    for (uint32_t i = 0; i < key_len; i++) {
        hash = ((hash << 5) + hash) + key[i];
    }
    return hash;
}

// 根据键计算分片索引
static uint32_t get_shard_index(const sharded_memtable_t* table,
                              const char* key, uint32_t key_len) {
    uint32_t hash = hash_key(key, key_len);
    return hash & ((1 << table->config.shard_bits) - 1);
}

// 创建分片内存表
sharded_memtable_t* sharded_memtable_create(const shard_config_t* config) {
    sharded_memtable_t* table = (sharded_memtable_t*)malloc(sizeof(sharded_memtable_t));
    if (!table) {
        ppdb_log_error("Failed to allocate sharded memtable");
        return NULL;
    }

    // 复制配置
    memcpy(&table->config, config, sizeof(shard_config_t));

    // 分配分片数组
    table->shards = (atomic_skiplist_t**)malloc(config->shard_count * sizeof(atomic_skiplist_t*));
    if (!table->shards) {
        free(table);
        ppdb_log_error("Failed to allocate shards array");
        return NULL;
    }

    // 初始化每个分片
    for (uint32_t i = 0; i < config->shard_count; i++) {
        table->shards[i] = atomic_skiplist_create(MAX_LEVEL);
        if (!table->shards[i]) {
            for (uint32_t j = 0; j < i; j++) {
                atomic_skiplist_destroy(table->shards[j]);
            }
            free(table->shards);
            free(table);
            ppdb_log_error("Failed to create skiplist for shard %u", i);
            return NULL;
        }
    }

    atomic_init(&table->total_size, 0);
    return table;
}

// 销毁分片内存表
void sharded_memtable_destroy(sharded_memtable_t* table) {
    if (!table) return;

    // 销毁每个分片
    for (uint32_t i = 0; i < table->config.shard_count; i++) {
        atomic_skiplist_destroy(table->shards[i]);
    }

    free(table->shards);
    free(table);
}

// 插入键值对
bool sharded_memtable_put(sharded_memtable_t* table, const char* key, uint32_t key_len,
                         const void* value, uint32_t value_len) {
    uint32_t shard_index = get_shard_index(table, key, key_len);
    atomic_skiplist_t* shard = table->shards[shard_index];

    // 检查分片大小是否超过限制
    if (atomic_skiplist_size(shard) >= table->config.max_size) {
        ppdb_log_error("Shard %u is full", shard_index);
        return false;
    }

    if (atomic_skiplist_insert(shard, key, key_len, value, value_len)) {
        atomic_fetch_add_explicit(&table->total_size, 1, memory_order_relaxed);
        return true;
    }
    return false;
}

// 删除键值对
bool sharded_memtable_delete(sharded_memtable_t* table, const char* key, uint32_t key_len) {
    uint32_t shard_index = get_shard_index(table, key, key_len);
    atomic_skiplist_t* shard = table->shards[shard_index];

    if (atomic_skiplist_delete(shard, key, key_len)) {
        atomic_fetch_sub_explicit(&table->total_size, 1, memory_order_relaxed);
        return true;
    }
    return false;
}

// 查找键值对
bool sharded_memtable_get(sharded_memtable_t* table, const char* key, uint32_t key_len,
                         void** value, uint32_t* value_len) {
    uint32_t shard_index = get_shard_index(table, key, key_len);
    atomic_skiplist_t* shard = table->shards[shard_index];
    return atomic_skiplist_find(shard, key, key_len, value, value_len);
}

// 获取总元素个数
uint32_t sharded_memtable_size(sharded_memtable_t* table) {
    return atomic_load_explicit(&table->total_size, memory_order_relaxed);
}

// 获取指定分片的元素个数
uint32_t sharded_memtable_shard_size(sharded_memtable_t* table, uint32_t shard_index) {
    if (shard_index >= table->config.shard_count) return 0;
    return atomic_skiplist_size(table->shards[shard_index]);
}

// 清空分片内存表
void sharded_memtable_clear(sharded_memtable_t* table) {
    for (uint32_t i = 0; i < table->config.shard_count; i++) {
        atomic_skiplist_clear(table->shards[i]);
    }
    atomic_store_explicit(&table->total_size, 0, memory_order_relaxed);
}

// 遍历分片内存表
void sharded_memtable_foreach(sharded_memtable_t* table, memtable_visitor_t visitor, void* ctx) {
    for (uint32_t i = 0; i < table->config.shard_count; i++) {
        atomic_skiplist_foreach(table->shards[i], visitor, ctx);
    }
}
