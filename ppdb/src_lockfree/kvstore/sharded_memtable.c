#include <stdlib.h>
#include <string.h>
#include "sharded_memtable.h"

// 创建分片式 MemTable
ppdb_sharded_memtable_t* ppdb_sharded_memtable_create(
    size_t shard_count,
    size_t shard_size_limit) {
    
    ppdb_sharded_memtable_t* table = malloc(sizeof(ppdb_sharded_memtable_t));
    if (!table) return NULL;

    table->shard_count = shard_count;
    table->shards = malloc(shard_count * sizeof(ppdb_memtable_shard_t));
    if (!table->shards) {
        free(table);
        return NULL;
    }

    atomic_store(&table->total_size, 0);
    atomic_store(&table->next_shard_index, 0);

    // 初始化每个分片
    for (size_t i = 0; i < shard_count; i++) {
        table->shards[i].list = atomic_skiplist_create(32);  // 最大32层
        if (!table->shards[i].list) {
            // 清理已创建的分片
            for (size_t j = 0; j < i; j++) {
                atomic_skiplist_destroy(table->shards[j].list);
            }
            free(table->shards);
            free(table);
            return NULL;
        }
        table->shards[i].size_limit = shard_size_limit;
        atomic_store(&table->shards[i].current_size, 0);
        atomic_store(&table->shards[i].is_immutable, false);
    }

    return table;
}

// 销毁分片式 MemTable
void ppdb_sharded_memtable_destroy(ppdb_sharded_memtable_t* table) {
    if (!table) return;

    if (table->shards) {
        for (size_t i = 0; i < table->shard_count; i++) {
            atomic_skiplist_destroy(table->shards[i].list);
        }
        free(table->shards);
    }
    free(table);
}

// 计算键的分片索引
static size_t get_shard_index(ppdb_sharded_memtable_t* table,
                            const uint8_t* key, size_t key_len) {
    // TODO: 实现更好的分片策略
    // 当前简单地对键的第一个字节取模
    return key[0] % table->shard_count;
}

// 写入键值对
int ppdb_sharded_memtable_put(ppdb_sharded_memtable_t* table,
                             const uint8_t* key, size_t key_len,
                             const uint8_t* value, size_t value_len) {
    if (!table || !key || !value) return -1;

    size_t shard_idx = get_shard_index(table, key, key_len);
    ppdb_memtable_shard_t* shard = &table->shards[shard_idx];

    // 检查分片是否为只读
    if (atomic_load(&shard->is_immutable)) {
        return -1;
    }

    // 检查大小限制
    size_t entry_size = key_len + value_len;
    size_t new_size = atomic_fetch_add(&shard->current_size, entry_size);
    if (new_size + entry_size > shard->size_limit) {
        atomic_fetch_sub(&shard->current_size, entry_size);
        return -1;
    }

    // 写入跳表
    int ret = atomic_skiplist_put(shard->list, key, key_len, value, value_len);
    if (ret != 0) {
        atomic_fetch_sub(&shard->current_size, entry_size);
        return ret;
    }

    atomic_fetch_add(&table->total_size, entry_size);
    return 0;
}

// 读取键值对
int ppdb_sharded_memtable_get(ppdb_sharded_memtable_t* table,
                             const uint8_t* key, size_t key_len,
                             uint8_t* value, size_t* value_len) {
    if (!table || !key || !value || !value_len) return -1;

    size_t shard_idx = get_shard_index(table, key, key_len);
    ppdb_memtable_shard_t* shard = &table->shards[shard_idx];

    return atomic_skiplist_get(shard->list, key, key_len, value, value_len);
}

// 删除键值对
int ppdb_sharded_memtable_delete(ppdb_sharded_memtable_t* table,
                                const uint8_t* key, size_t key_len) {
    if (!table || !key) return -1;

    size_t shard_idx = get_shard_index(table, key, key_len);
    ppdb_memtable_shard_t* shard = &table->shards[shard_idx];

    // 检查分片是否为只读
    if (atomic_load(&shard->is_immutable)) {
        return -1;
    }

    return atomic_skiplist_delete(shard->list, key, key_len);
}

// 获取总大小
size_t ppdb_sharded_memtable_size(ppdb_sharded_memtable_t* table) {
    if (!table) return 0;
    return atomic_load(&table->total_size);
}

// 将分片转换为只读状态
int ppdb_sharded_memtable_make_immutable(ppdb_sharded_memtable_t* table,
                                        size_t shard_index) {
    if (!table || shard_index >= table->shard_count) return -1;

    bool expected = false;
    bool desired = true;
    
    // 原子地将分片设置为只读状态
    if (atomic_compare_exchange_strong(&table->shards[shard_index].is_immutable,
                                     &expected, desired)) {
        return 0;
    }
    return -1;  // 已经是只读状态
}

// 检查分片是否为只读状态
bool ppdb_sharded_memtable_is_immutable(ppdb_sharded_memtable_t* table,
                                       size_t shard_index) {
    if (!table || shard_index >= table->shard_count) return true;
    return atomic_load(&table->shards[shard_index].is_immutable);
}
