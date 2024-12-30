#ifndef PPDB_KVSTORE_MEMTABLE_H
#define PPDB_KVSTORE_MEMTABLE_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/skiplist.h"

// 内存表配置
typedef struct ppdb_memtable_config {
    ppdb_memtable_type_t type;  // 内存表类型
    size_t size_limit;          // 大小限制
    size_t shard_count;         // 分片数量
    ppdb_sync_config_t sync;    // 同步配置
} ppdb_memtable_config_t;

// 函数声明
// 初始化内存表
ppdb_error_t ppdb_memtable_init(ppdb_memtable_t* memtable, const ppdb_memtable_config_t* config);

// 销毁内存表
void ppdb_memtable_destroy(ppdb_memtable_t* memtable);

// 插入键值对
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* memtable, const void* key, size_t key_len, const void* value, size_t value_len);

// 获取键值对
ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* memtable, const void* key, size_t key_len, void* value, size_t* value_len);

// 删除键值对
ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* memtable, const void* key, size_t key_len);

// 清空内存表
void ppdb_memtable_clear(ppdb_memtable_t* memtable);

// 获取内存表大小
size_t ppdb_memtable_size(ppdb_memtable_t* memtable);

// 检查内存表是否为空
bool ppdb_memtable_empty(ppdb_memtable_t* memtable);

// 检查内存表是否已满
bool ppdb_memtable_full(ppdb_memtable_t* memtable);

// 创建内存表
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** memtable);
ppdb_error_t ppdb_memtable_create_sharded(size_t size_limit, ppdb_memtable_t** memtable);
ppdb_error_t ppdb_memtable_create_lockfree(size_t size_limit, ppdb_memtable_t** memtable);

// 无锁操作
ppdb_error_t ppdb_memtable_put_lockfree(ppdb_memtable_t* memtable, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_memtable_get_lockfree(ppdb_memtable_t* memtable, const void* key, size_t key_len, void* value, size_t* value_len);
ppdb_error_t ppdb_memtable_delete_lockfree(ppdb_memtable_t* memtable, const void* key, size_t key_len);

// 迭代器操作
ppdb_error_t ppdb_memtable_iterator_create(ppdb_memtable_t* memtable, ppdb_memtable_iterator_t** iter);
ppdb_error_t ppdb_memtable_iterator_next(ppdb_memtable_iterator_t* iter, void** key, size_t* key_len, void** value, size_t* value_len);
void ppdb_memtable_iterator_destroy(ppdb_memtable_iterator_t* iter);

// 内存表状态
size_t ppdb_memtable_max_size(ppdb_memtable_t* memtable);
bool ppdb_memtable_is_immutable(ppdb_memtable_t* memtable);
void ppdb_memtable_set_immutable(ppdb_memtable_t* memtable, bool immutable);

#endif // PPDB_KVSTORE_MEMTABLE_H 