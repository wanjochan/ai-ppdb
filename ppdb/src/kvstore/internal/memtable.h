#ifndef PPDB_MEMTABLE_H
#define PPDB_MEMTABLE_H

#include "sync.h"
#include "skiplist.h"

// MemTable配置
typedef struct ppdb_memtable_config {
    ppdb_sync_config_t sync_config;    // 同步配置
    size_t max_size;                  // 最大内存使用
    uint32_t max_level;               // 最大层数
    bool enable_compression;          // 启用压缩
    bool enable_bloom_filter;         // 启用布隆过滤器
} ppdb_memtable_config_t;

// MemTable结构
typedef struct ppdb_memtable ppdb_memtable_t;

// 创建MemTable
ppdb_memtable_t* ppdb_memtable_create(const ppdb_memtable_config_t* config);

// 销毁MemTable
void ppdb_memtable_destroy(ppdb_memtable_t* table);

// 写入数据
int ppdb_memtable_put(ppdb_memtable_t* table, const void* key, size_t key_len,
                      const void* value, size_t value_len);

// 读取数据
int ppdb_memtable_get(ppdb_memtable_t* table, const void* key, size_t key_len,
                      void** value, size_t* value_len);

// 删除数据
int ppdb_memtable_delete(ppdb_memtable_t* table, const void* key, size_t key_len);

// 转换为不可变表
void ppdb_memtable_make_immutable(ppdb_memtable_t* table);

// 检查是否为不可变表
bool ppdb_memtable_is_immutable(ppdb_memtable_t* table);

// 获取内存使用量
size_t ppdb_memtable_memory_usage(ppdb_memtable_t* table);

// 获取数据条数
size_t ppdb_memtable_size(ppdb_memtable_t* table);

// 获取性能指标
ppdb_metrics_t* ppdb_memtable_get_metrics(ppdb_memtable_t* table);

#endif // PPDB_MEMTABLE_H
