#ifndef PPDB_SKIPLIST_UNIFIED_H
#define PPDB_SKIPLIST_UNIFIED_H

#include <stddef.h>
#include <stdbool.h>
#include "../common/sync_unified.h"

// 跳表节点
typedef struct ppdb_skiplist_node {
    uint32_t level;
    size_t key_len;
    size_t value_len;
    struct ppdb_skiplist_node* next[];  // 柔性数组
    // key和value数据紧随其后
} ppdb_skiplist_node_t;

// 跳表配置
typedef struct ppdb_skiplist_config {
    ppdb_sync_config_t sync_config;  // 同步配置
    bool enable_hint;                // 是否启用查找提示
    size_t max_size;                // 最大内存限制
    uint32_t max_level;             // 最大层数
} ppdb_skiplist_config_t;

// 跳表结构
typedef struct ppdb_skiplist {
    ppdb_skiplist_node_t* head;     // 头节点
    uint32_t max_level;             // 最大层数
    size_t size;                    // 元素个数
    
    // 同步机制
    union {
        ppdb_sync_t global_lock;    // 全局锁
        ppdb_stripe_locks_t* stripes; // 分片锁
    } sync;
    
    // 配置
    ppdb_skiplist_config_t config;
    
    // 性能优化
    struct {
        struct skip_hint_t* hints;  // 查找提示缓存
    } opt;
    
    // 统计信息
    struct {
        atomic_size_t mem_used;     // 内存使用
        atomic_uint64_t ops_count;  // 操作计数
        atomic_uint64_t conflicts;  // 冲突计数
    } stats;
} ppdb_skiplist_t;

// API函数
ppdb_skiplist_t* ppdb_skiplist_create(const ppdb_skiplist_config_t* config);
void ppdb_skiplist_destroy(ppdb_skiplist_t* list);

int ppdb_skiplist_insert(ppdb_skiplist_t* list,
                        const void* key, size_t key_len,
                        const void* value, size_t value_len);

int ppdb_skiplist_remove(ppdb_skiplist_t* list,
                        const void* key, size_t key_len);

int ppdb_skiplist_find(ppdb_skiplist_t* list,
                      const void* key, size_t key_len,
                      void** value, size_t* value_len);

// 迭代器支持
typedef struct ppdb_skiplist_iter {
    ppdb_skiplist_t* list;
    ppdb_skiplist_node_t* current;
    void* key_buf;
    size_t key_size;
} ppdb_skiplist_iter_t;

ppdb_skiplist_iter_t* ppdb_skiplist_iter_create(ppdb_skiplist_t* list);
void ppdb_skiplist_iter_destroy(ppdb_skiplist_iter_t* iter);
bool ppdb_skiplist_iter_valid(ppdb_skiplist_iter_t* iter);
void ppdb_skiplist_iter_next(ppdb_skiplist_iter_t* iter);
const void* ppdb_skiplist_iter_key(ppdb_skiplist_iter_t* iter, size_t* len);
const void* ppdb_skiplist_iter_value(ppdb_skiplist_iter_t* iter, size_t* len);

#endif // PPDB_SKIPLIST_UNIFIED_H
