#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_logger.h"
#include "kvstore/internal/kvstore_fs.h"
#include "kvstore/internal/skiplist.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/metrics.h"
#include "internal/kvstore_sharded_memtable.h"

// 跳表节点大小估计
#define PPDB_SKIPLIST_NODE_SIZE 64
#define PPDB_DEFAULT_SHARD_COUNT 16

// 获取分片索引
static size_t get_shard_index(const void* key, size_t key_len, size_t shard_count) {
    uint32_t hash = 0;
    const uint8_t* bytes = (const uint8_t*)key;
    for (size_t i = 0; i < key_len; i++) {
        hash = hash * 31 + bytes[i];
    }
    return hash % shard_count;
}

// 创建分片内存表
ppdb_error_t ppdb_memtable_create_sharded_basic(size_t size_limit, ppdb_memtable_t** table) {
    if (!table) return PPDB_ERR_NULL_POINTER;

    // 分配内存表结构
    ppdb_memtable_t* new_table = aligned_alloc(64, sizeof(ppdb_memtable_t));
    if (!new_table) return PPDB_ERR_NO_MEMORY;

    // 初始化基本字段
    new_table->type = PPDB_MEMTABLE_SHARDED;
    new_table->size_limit = size_limit;
    new_table->current_size = 0;
    new_table->shard_count = PPDB_DEFAULT_SHARD_COUNT;
    new_table->is_immutable = false;
    memset(&new_table->metrics, 0, sizeof(ppdb_metrics_t));

    // 分配分片数组
    new_table->shards = calloc(new_table->shard_count, sizeof(ppdb_memtable_shard_t));
    if (!new_table->shards) {
        free(new_table);
        return PPDB_ERR_NO_MEMORY;
    }

    // 初始化每个分片
    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000
    };

    for (size_t i = 0; i < new_table->shard_count; i++) {
        // 创建分片的跳表
        ppdb_error_t err = ppdb_skiplist_create(&new_table->shards[i].skiplist);
        if (err != PPDB_OK) {
            // 清理已创建的分片
            for (size_t j = 0; j < i; j++) {
                ppdb_skiplist_destroy(new_table->shards[j].skiplist);
                ppdb_sync_destroy(&new_table->shards[j].sync);
            }
            free(new_table->shards);
            free(new_table);
            return err;
        }

        // 初始化分片的同步原语
        err = ppdb_sync_init(&new_table->shards[i].sync, &sync_config);
        if (err != PPDB_OK) {
            ppdb_skiplist_destroy(new_table->shards[i].skiplist);
            // 清理已创建的分片
            for (size_t j = 0; j < i; j++) {
                ppdb_skiplist_destroy(new_table->shards[j].skiplist);
                ppdb_sync_destroy(&new_table->shards[j].sync);
            }
            free(new_table->shards);
            free(new_table);
            return err;
        }

        // 初始化分片大小
        new_table->shards[i].size = 0;
    }

    *table = new_table;
    return PPDB_OK;
}

// 销毁分片内存表
void ppdb_memtable_destroy_sharded(ppdb_memtable_t* table) {
    if (!table || !table->shards) return;

    // 清理所有分片
    for (size_t i = 0; i < table->shard_count; i++) {
        ppdb_skiplist_destroy(table->shards[i].skiplist);
        ppdb_sync_destroy(&table->shards[i].sync);
    }

    free(table->shards);
    free(table);
}

// 写入键值对到分片内存表
ppdb_error_t ppdb_memtable_put_sharded(ppdb_memtable_t* table,
                                      const void* key, size_t key_len,
                                      const void* value, size_t value_len) {
    if (!table || !table->shards || !key || !value) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0 || value_len == 0) return PPDB_ERR_INVALID_ARGUMENT;

    size_t total_size = key_len + value_len + PPDB_SKIPLIST_NODE_SIZE;
    if (table->current_size + total_size > table->size_limit) {
        return PPDB_ERR_MEMTABLE_FULL;
    }

    // 获取目标分片
    size_t shard_index = get_shard_index(key, key_len, table->shard_count);
    ppdb_memtable_shard_t* shard = &table->shards[shard_index];

    // 加锁访问分片
    ppdb_sync_lock(&shard->sync);

    // 写入数据
    ppdb_error_t err = ppdb_skiplist_put(shard->skiplist,
                                        key, key_len,
                                        value, value_len);
    if (err == PPDB_OK) {
        atomic_fetch_add(&shard->size, total_size);
        atomic_fetch_add(&table->current_size, total_size);
    }

    ppdb_sync_unlock(&shard->sync);
    return err;
}

// 从分片内存表读取键值对
ppdb_error_t ppdb_memtable_get_sharded(ppdb_memtable_t* table,
                                      const void* key, size_t key_len,
                                      void** value, size_t* value_len) {
    if (!table || !table->shards || !key || !value || !value_len) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARGUMENT;

    // 获取目标分片
    size_t shard_index = get_shard_index(key, key_len, table->shard_count);
    ppdb_memtable_shard_t* shard = &table->shards[shard_index];

    // 加锁访问分片
    ppdb_sync_lock(&shard->sync);

    // 读取数据
    ppdb_error_t err = ppdb_skiplist_get(shard->skiplist,
                                        key, key_len,
                                        value, value_len);

    ppdb_sync_unlock(&shard->sync);
    return err;
}

// 从分片内存表删除键值对
ppdb_error_t ppdb_memtable_delete_sharded(ppdb_memtable_t* table,
                                         const void* key, size_t key_len) {
    if (!table || !table->shards || !key) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARGUMENT;

    // 获取目标分片
    size_t shard_index = get_shard_index(key, key_len, table->shard_count);
    ppdb_memtable_shard_t* shard = &table->shards[shard_index];

    // 加锁访问分片
    ppdb_sync_lock(&shard->sync);

    // 删除数据
    ppdb_error_t err = ppdb_skiplist_delete(shard->skiplist,
                                           key, key_len);
    if (err == PPDB_OK) {
        atomic_fetch_sub(&shard->size, key_len + PPDB_SKIPLIST_NODE_SIZE);
        atomic_fetch_sub(&table->current_size, key_len + PPDB_SKIPLIST_NODE_SIZE);
    }

    ppdb_sync_unlock(&shard->sync);
    return err;
}
