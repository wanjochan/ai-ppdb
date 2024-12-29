#ifndef PPDB_SKIPLIST_H
#define PPDB_SKIPLIST_H

#include "ppdb/common/sync.h"

// 跳表配置
typedef struct ppdb_skiplist_config {
    ppdb_sync_config_t sync_config;    // 同步配置
    bool enable_hint;                  // 启用查找提示
    size_t max_size;                  // 最大内存使用
    uint32_t max_level;               // 最大层数
} ppdb_skiplist_config_t;

// 跳表结构
typedef struct ppdb_skiplist ppdb_skiplist_t;

// 跳表迭代器
typedef struct ppdb_skiplist_iter ppdb_skiplist_iter_t;

// 创建跳表
ppdb_skiplist_t* ppdb_skiplist_create(const ppdb_skiplist_config_t* config);

// 销毁跳表
void ppdb_skiplist_destroy(ppdb_skiplist_t* list);

// 插入数据
int ppdb_skiplist_insert(ppdb_skiplist_t* list, const void* key, size_t key_len,
                        const void* value, size_t value_len);

// 查找数据
int ppdb_skiplist_find(ppdb_skiplist_t* list, const void* key, size_t key_len,
                      void** value, size_t* value_len);

// 删除数据
int ppdb_skiplist_remove(ppdb_skiplist_t* list, const void* key, size_t key_len);

// 创建迭代器
ppdb_skiplist_iter_t* ppdb_skiplist_iter_create(ppdb_skiplist_t* list);

// 销毁迭代器
void ppdb_skiplist_iter_destroy(ppdb_skiplist_iter_t* iter);

// 迭代器是否有效
bool ppdb_skiplist_iter_valid(ppdb_skiplist_iter_t* iter);

// 迭代器移动到下一个
void ppdb_skiplist_iter_next(ppdb_skiplist_iter_t* iter);

// 获取迭代器当前key
int ppdb_skiplist_iter_key(ppdb_skiplist_iter_t* iter, void** key, size_t* key_len);

// 获取迭代器当前value
int ppdb_skiplist_iter_value(ppdb_skiplist_iter_t* iter, void** value, size_t* value_len);

#endif // PPDB_SKIPLIST_H
