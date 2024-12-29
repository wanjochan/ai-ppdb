#ifndef PPDB_MEMTABLE_UNIFIED_H
#define PPDB_MEMTABLE_UNIFIED_H

#include "../skiplist/skiplist_unified.h"
#include "../common/sync_unified.h"

// MemTable配置
typedef struct ppdb_memtable_config {
    ppdb_sync_config_t sync_config;    // 同步配置
    size_t max_size;                   // 最大内存限制
    uint32_t max_level;                // 跳表最大层数
    bool enable_compression;           // 是否启用压缩
    bool enable_bloom_filter;          // 是否启用布隆过滤器
} ppdb_memtable_config_t;

// MemTable结构
typedef struct ppdb_memtable {
    ppdb_skiplist_t* skiplist;         // 底层跳表
    uint64_t sequence;                 // 序列号
    
    // 配置
    ppdb_memtable_config_t config;
    
    // 优化结构
    struct {
        void* bloom_filter;            // 布隆过滤器
        void* compress_ctx;            // 压缩上下文
    } opt;
    
    // 统计信息
    struct {
        atomic_size_t mem_used;        // 内存使用
        atomic_uint64_t inserts;       // 插入次数
        atomic_uint64_t deletes;       // 删除次数
        atomic_uint64_t updates;       // 更新次数
        atomic_uint64_t conflicts;     // 冲突次数
    } stats;
    
    // 状态标志
    atomic_bool is_immutable;          // 是否不可变
} ppdb_memtable_t;

// API函数
ppdb_memtable_t* ppdb_memtable_create(const ppdb_memtable_config_t* config);
void ppdb_memtable_destroy(ppdb_memtable_t* table);

int ppdb_memtable_put(ppdb_memtable_t* table,
                      const void* key, size_t key_len,
                      const void* value, size_t value_len);

int ppdb_memtable_get(ppdb_memtable_t* table,
                      const void* key, size_t key_len,
                      void** value, size_t* value_len);

int ppdb_memtable_delete(ppdb_memtable_t* table,
                        const void* key, size_t key_len);

// 迭代器支持
typedef struct ppdb_memtable_iter {
    ppdb_skiplist_iter_t* skiplist_iter;
    ppdb_memtable_t* table;
} ppdb_memtable_iter_t;

ppdb_memtable_iter_t* ppdb_memtable_iter_create(ppdb_memtable_t* table);
void ppdb_memtable_iter_destroy(ppdb_memtable_iter_t* iter);
bool ppdb_memtable_iter_valid(ppdb_memtable_iter_t* iter);
void ppdb_memtable_iter_next(ppdb_memtable_iter_t* iter);
const void* ppdb_memtable_iter_key(ppdb_memtable_iter_t* iter, size_t* len);
const void* ppdb_memtable_iter_value(ppdb_memtable_iter_t* iter, size_t* len);

// 快照和刷盘
void ppdb_memtable_make_immutable(ppdb_memtable_t* table);
bool ppdb_memtable_is_immutable(ppdb_memtable_t* table);
size_t ppdb_memtable_memory_usage(ppdb_memtable_t* table);

#endif // PPDB_MEMTABLE_UNIFIED_H
