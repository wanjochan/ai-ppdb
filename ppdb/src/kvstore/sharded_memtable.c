#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "internal/kvstore_types.h"
#include "internal/kvstore_memtable.h"
#include "internal/kvstore_logger.h"
#include "internal/kvstore_fs.h"
#include "internal/skiplist.h"
#include "internal/sync.h"
#include "internal/metrics.h"

// 计算哈希值
static uint32_t ppdb_hash(const void* key, size_t key_len) {
    const uint8_t* bytes = (const uint8_t*)key;
    uint32_t hash = 5381;
    for (size_t i = 0; i < key_len; i++) {
        hash = ((hash << 5) + hash) + bytes[i];
    }
    return hash;
}

// 分片配置
typedef struct ppdb_shard_config {
    uint32_t shard_bits;      // 分片位数
    size_t shard_size;        // 每个分片大小
    bool use_lockfree;        // 是否使用无锁模式
} ppdb_shard_config_t;

// 分片内存表结构
struct ppdb_memtable {
    ppdb_shard_config_t config;       // 分片配置
    ppdb_skiplist_t** shards;         // 分片数组
    ppdb_sync_t* shard_syncs;         // 分片锁数组
    ppdb_sync_t sync;                 // 全局锁
    _Atomic(size_t) total_size;       // 总大小
    _Atomic(bool) is_immutable;       // 是否不可变
    ppdb_metrics_t metrics;           // 性能指标
};

// 获取分片索引
static uint32_t get_shard_index(const ppdb_memtable_t* table,
                               const void* key, size_t key_len) {
    uint32_t hash = ppdb_hash(key, key_len);
    return hash & ((1 << table->config.shard_bits) - 1);
}

// 创建分片内存表
ppdb_error_t ppdb_memtable_create_sharded(size_t size, ppdb_memtable_t** table) {
    if (!table) return PPDB_ERR_NULL_POINTER;
    if (size == 0) return PPDB_ERR_INVALID_ARG;

    // 分配内存表结构
    ppdb_memtable_t* new_table = aligned_alloc(64, sizeof(ppdb_memtable_t));
    if (!new_table) return PPDB_ERR_NO_MEMORY;

    // 初始化分片配置
    new_table->config.shard_bits = 4;  // 默认16个分片
    new_table->config.shard_size = size / (1 << new_table->config.shard_bits);
    new_table->config.use_lockfree = false;

    // 初始化全局锁
    ppdb_sync_config_t sync_config = {
        .use_lockfree = false,
        .stripe_count = 0,
        .spin_count = 1000,
        .backoff_us = 100,
        .enable_ref_count = false
    };
    ppdb_error_t err = ppdb_sync_init(&new_table->sync, &sync_config);
    if (err != PPDB_OK) {
        free(new_table);
        return err;
    }

    // 分配分片数组
    uint32_t shard_count = 1 << new_table->config.shard_bits;
    new_table->shards = aligned_alloc(64, shard_count * sizeof(ppdb_skiplist_t*));
    if (!new_table->shards) {
        ppdb_sync_destroy(&new_table->sync);
        free(new_table);
        return PPDB_ERR_NO_MEMORY;
    }

    // 分配分片锁数组
    new_table->shard_syncs = aligned_alloc(64, shard_count * sizeof(ppdb_sync_t));
    if (!new_table->shard_syncs) {
        ppdb_sync_destroy(&new_table->sync);
        free(new_table->shards);
        free(new_table);
        return PPDB_ERR_NO_MEMORY;
    }

    // 初始化分片
    for (uint32_t i = 0; i < shard_count; i++) {
        // 初始化分片锁
        err = ppdb_sync_init(&new_table->shard_syncs[i], &sync_config);
        if (err != PPDB_OK) {
            for (uint32_t j = 0; j < i; j++) {
                ppdb_skiplist_destroy(new_table->shards[j]);
                ppdb_sync_destroy(&new_table->shard_syncs[j]);
            }
            ppdb_sync_destroy(&new_table->sync);
            free(new_table->shard_syncs);
            free(new_table->shards);
            free(new_table);
            return err;
        }

        // 创建分片跳表
        err = ppdb_skiplist_create(&new_table->shards[i]);
        if (err != PPDB_OK) {
            for (uint32_t j = 0; j < i; j++) {
                ppdb_skiplist_destroy(new_table->shards[j]);
                ppdb_sync_destroy(&new_table->shard_syncs[j]);
            }
            ppdb_sync_destroy(&new_table->shard_syncs[i]);
            ppdb_sync_destroy(&new_table->sync);
            free(new_table->shard_syncs);
            free(new_table->shards);
            free(new_table);
            return err;
        }
    }

    // 初始化其他字段
    atomic_init(&new_table->total_size, 0);
    atomic_init(&new_table->is_immutable, false);
    ppdb_metrics_init(&new_table->metrics);

    *table = new_table;
    return PPDB_OK;
}

// 销毁分片内存表
void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    if (!table) return;

    uint32_t shard_count = 1 << table->config.shard_bits;
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

// 写入键值对
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              const void* value, size_t value_len) {
    if (!table || !key || !value) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0 || value_len == 0) return PPDB_ERR_INVALID_ARG;

    // 开始监控
    uint64_t start_time = now_us();

    // 检查是否可写
    if (atomic_load(&table->is_immutable)) {
        ppdb_metrics_record_op(&table->metrics, 0);
        return PPDB_ERR_IMMUTABLE;
    }

    // 获取分片索引
    uint32_t shard_index = get_shard_index(table, key, key_len);

    // 检查分片大小
    size_t entry_size = key_len + value_len + 64;  // 估计每个节点的开销
    if (atomic_load(&table->total_size) + entry_size > table->config.shard_size * (1 << table->config.shard_bits)) {
        ppdb_metrics_record_op(&table->metrics, 0);
        return PPDB_ERR_FULL;
    }

    // 加锁
    ppdb_sync_lock(&table->shard_syncs[shard_index]);

    // 写入跳表
    ppdb_error_t err = ppdb_skiplist_put(table->shards[shard_index],
                                        (const uint8_t*)key, key_len,
                                        (const uint8_t*)value, value_len);
    if (err == PPDB_OK) {
        atomic_fetch_add(&table->total_size, entry_size);
        ppdb_metrics_record_data(&table->metrics, key_len, value_len);
    }

    // 解锁
    ppdb_sync_unlock(&table->shard_syncs[shard_index]);

    ppdb_metrics_record_op(&table->metrics, now_us() - start_time);
    return err;
}

// 读取键值对
ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              void** value, size_t* value_len) {
    if (!table || !key || !value || !value_len) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARG;

    // 开始监控
    uint64_t start_time = now_us();

    // 获取分片索引
    uint32_t shard_index = get_shard_index(table, key, key_len);

    // 加锁
    ppdb_sync_lock(&table->shard_syncs[shard_index]);

    // 从跳表读取
    ppdb_error_t err = ppdb_skiplist_get(table->shards[shard_index],
                                        (const uint8_t*)key, key_len,
                                        (uint8_t**)value, value_len);

    // 解锁
    ppdb_sync_unlock(&table->shard_syncs[shard_index]);

    ppdb_metrics_record_op(&table->metrics, now_us() - start_time);
    return err;
}

// 删除键值对
ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const void* key, size_t key_len) {
    if (!table || !key) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARG;

    // 开始监控
    uint64_t start_time = now_us();

    // 检查是否可写
    if (atomic_load(&table->is_immutable)) {
        ppdb_metrics_record_op(&table->metrics, 0);
        return PPDB_ERR_IMMUTABLE;
    }

    // 获取分片索引
    uint32_t shard_index = get_shard_index(table, key, key_len);

    // 加锁
    ppdb_sync_lock(&table->shard_syncs[shard_index]);

    // 从跳表删除
    ppdb_error_t err = ppdb_skiplist_delete(table->shards[shard_index],
                                           (const uint8_t*)key, key_len);

    // 解锁
    ppdb_sync_unlock(&table->shard_syncs[shard_index]);

    ppdb_metrics_record_op(&table->metrics, now_us() - start_time);
    return err;
}

// 获取当前大小
size_t ppdb_memtable_size(ppdb_memtable_t* table) {
    if (!table) return 0;
    return atomic_load(&table->total_size);
}

// 获取最大大小
size_t ppdb_memtable_max_size(ppdb_memtable_t* table) {
    if (!table) return 0;
    return table->config.shard_size * (1 << table->config.shard_bits);
}

// 检查是否不可变
bool ppdb_memtable_is_immutable(ppdb_memtable_t* table) {
    if (!table) return false;
    return atomic_load(&table->is_immutable);
}

// 设置为不可变
void ppdb_memtable_set_immutable(ppdb_memtable_t* table) {
    if (!table) return;
    atomic_store(&table->is_immutable, true);
}

// 获取性能指标
const ppdb_metrics_t* ppdb_memtable_get_metrics(ppdb_memtable_t* table) {
    if (!table) return NULL;
    return &table->metrics;
}
