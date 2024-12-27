#ifndef PPDB_SHARDED_MEMTABLE_H
#define PPDB_SHARDED_MEMTABLE_H

#include <cosmopolitan.h>
#include "atomic_skiplist.h"

// 分片的 MemTable
typedef struct ppdb_memtable_shard_t {
    atomic_skiplist_t* list;
    size_t size_limit;
    atomic_size_t current_size;
    atomic_bool is_immutable;     // 是否为只读状态
} ppdb_memtable_shard_t;

// 分片式 MemTable
typedef struct ppdb_sharded_memtable_t {
    size_t shard_count;           // 分片数量
    ppdb_memtable_shard_t* shards;  // 分片数组
    atomic_size_t total_size;     // 总大小
    atomic_uint next_shard_index; // 下一个写入分片的索引
} ppdb_sharded_memtable_t;

// 创建分片式 MemTable
ppdb_sharded_memtable_t* ppdb_sharded_memtable_create(
    size_t shard_count,
    size_t shard_size_limit);

// 销毁分片式 MemTable
void ppdb_sharded_memtable_destroy(
    ppdb_sharded_memtable_t* table);

// 写入键值对
int ppdb_sharded_memtable_put(
    ppdb_sharded_memtable_t* table,
    const uint8_t* key, size_t key_len,
    const uint8_t* value, size_t value_len);

// 读取键值对
int ppdb_sharded_memtable_get(
    ppdb_sharded_memtable_t* table,
    const uint8_t* key, size_t key_len,
    uint8_t* value, size_t* value_len);

// 删除键值对
int ppdb_sharded_memtable_delete(
    ppdb_sharded_memtable_t* table,
    const uint8_t* key, size_t key_len);

// 获取总大小
size_t ppdb_sharded_memtable_size(
    ppdb_sharded_memtable_t* table);

// 将分片转换为只读状态
int ppdb_sharded_memtable_make_immutable(
    ppdb_sharded_memtable_t* table,
    size_t shard_index);

// 检查分片是否为只读状态
bool ppdb_sharded_memtable_is_immutable(
    ppdb_sharded_memtable_t* table,
    size_t shard_index);

#endif // PPDB_SHARDED_MEMTABLE_H
