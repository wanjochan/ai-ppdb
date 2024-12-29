#ifndef PPDB_SHARDED_MEMTABLE_H
#define PPDB_SHARDED_MEMTABLE_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/defs.h"

// 分片配置
typedef struct {
    uint32_t shard_bits;       // 分片位数
    uint32_t shard_count;      // 分片数量
    uint32_t max_size;         // 每个分片的最大大小
} shard_config_t;

// 前向声明
typedef struct sharded_memtable_t sharded_memtable_t;

// 创建分片内存表
sharded_memtable_t* sharded_memtable_create(const shard_config_t* config);

// 销毁分片内存表
void sharded_memtable_destroy(sharded_memtable_t* table);

// 插入键值对
int sharded_memtable_put(sharded_memtable_t* table, const uint8_t* key, size_t key_len,
                        const uint8_t* value, size_t value_len);

// 删除键值对
int sharded_memtable_delete(sharded_memtable_t* table, const uint8_t* key, size_t key_len);

// 查找键值对
int sharded_memtable_get(sharded_memtable_t* table, const uint8_t* key, size_t key_len,
                        uint8_t* value, size_t* value_len);

// 获取总元素个数
size_t sharded_memtable_size(sharded_memtable_t* table);

// 获取指定分片的元素个数
size_t sharded_memtable_shard_size(sharded_memtable_t* table, uint32_t shard_index);

// 清空分片内存表
void sharded_memtable_clear(sharded_memtable_t* table);

// 遍历分片内存表
typedef bool (*memtable_visitor_t)(const uint8_t* key, size_t key_len,
                                 const uint8_t* value, size_t value_len, void* ctx);
void sharded_memtable_foreach(sharded_memtable_t* table, memtable_visitor_t visitor, void* ctx);

#endif // PPDB_SHARDED_MEMTABLE_H
