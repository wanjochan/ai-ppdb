#ifndef PPDB_MEMTABLE_MUTEX_H
#define PPDB_MEMTABLE_MUTEX_H

#include <cosmopolitan.h>
#include "ppdb/error.h"

// MemTable结构
typedef struct ppdb_memtable_t ppdb_memtable_t;

// MemTable迭代器结构
typedef struct ppdb_memtable_iterator_t ppdb_memtable_iterator_t;

// 创建MemTable
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table);

// 销毁MemTable
void ppdb_memtable_destroy(ppdb_memtable_t* table);

// 写入键值对
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table,
                              const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len);

// 读取键值对
ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const uint8_t* key, size_t key_len,
                              uint8_t** value, size_t* value_len);

// 删除键值对
ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const uint8_t* key, size_t key_len);

// 获取当前大小
size_t ppdb_memtable_size(ppdb_memtable_t* table);

// 获取最大大小限制
size_t ppdb_memtable_max_size(ppdb_memtable_t* table);

// 复制数据到新的MemTable
ppdb_error_t ppdb_memtable_copy(ppdb_memtable_t* src, ppdb_memtable_t* dst);

// 创建迭代器
ppdb_error_t ppdb_memtable_iterator_create(ppdb_memtable_t* table,
                                         ppdb_memtable_iterator_t** iter);

// 销毁迭代器
void ppdb_memtable_iterator_destroy(ppdb_memtable_iterator_t* iter);

// 检查迭代器是否有效
bool ppdb_memtable_iterator_valid(ppdb_memtable_iterator_t* iter);

// 获取当前键
const uint8_t* ppdb_memtable_iterator_key(ppdb_memtable_iterator_t* iter);

// 获取当前值
const uint8_t* ppdb_memtable_iterator_value(ppdb_memtable_iterator_t* iter);

// 移动到下一个位置
void ppdb_memtable_iterator_next(ppdb_memtable_iterator_t* iter);

// 获取当前键值对
ppdb_error_t ppdb_memtable_iterator_get(ppdb_memtable_iterator_t* iter,
                                       const uint8_t** key, size_t* key_len,
                                       const uint8_t** value, size_t* value_len);

#endif // PPDB_MEMTABLE_MUTEX_H 