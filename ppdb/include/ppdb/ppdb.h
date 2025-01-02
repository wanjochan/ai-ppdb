#ifndef PPDB_H
#define PPDB_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_sync.h"

// 错误码定义
typedef enum {
    PPDB_OK = 0,                    // 成功
    PPDB_ERR_INVALID_ARG = -1,      // 无效参数
    PPDB_ERR_OUT_OF_MEMORY = -2,    // 内存不足
    PPDB_ERR_NOT_FOUND = -3,        // 未找到
    PPDB_ERR_ALREADY_EXISTS = -4,   // 已存在
    PPDB_ERR_NOT_SUPPORTED = -5,    // 不支持
    PPDB_ERR_IO = -6,               // IO错误
    PPDB_ERR_CORRUPTED = -7,        // 数据损坏
    PPDB_ERR_INTERNAL = -8,         // 内部错误
} ppdb_error_t;

// 统计信息
typedef struct {
    atomic_uint64_t get_count;      // Get操作总数
    atomic_uint64_t get_hits;       // Get命中次数
    atomic_uint64_t put_count;      // Put操作次数
    atomic_uint64_t remove_count;   // Remove操作次数
    atomic_uint64_t total_keys;     // 总键数
    atomic_uint64_t total_bytes;    // 总字节数
    atomic_uint64_t cache_hits;     // 缓存命中
    atomic_uint64_t cache_misses;   // 缓存未命中
} ppdb_metrics_t;

// 存储统计信息
typedef struct {
    ppdb_metrics_t base_metrics;    // 基础统计
    size_t memory_used;             // 内存使用
    size_t memory_allocated;        // 内存分配
    size_t block_count;             // 块数量
} ppdb_storage_stats_t;

// 类型标记
typedef enum {
    PPDB_TYPE_SKIPLIST = 1,    // 基础跳表
    PPDB_TYPE_MEMTABLE = 2,    // 内存表
    PPDB_TYPE_SHARDED = 4,     // 分片表
    PPDB_TYPE_WAL = 8,         // 预写日志
    PPDB_TYPE_SSTABLE = 16     // 有序表
} ppdb_type_t;

// 头部信息（4字节）
typedef struct {
    unsigned type : 4;        // 类型（16种）
    unsigned flags : 12;      // 状态标记
    unsigned refs : 16;       // 引用计数
} ppdb_header_t;

// 基础节点（24字节）
typedef struct {
    ppdb_header_t header;     // 4字节
    union {
        void* ptr;            // 通用指针
        uint64_t data;        // 内联数据
    };                        // 8字节
    void* extra;              // 额外数据，8字节
    uint32_t padding;         // 4字节(对齐)
} ppdb_node_t;

// 通用存储结构（24字节）
typedef struct {
    ppdb_header_t header;     // 4字节
    union {
        struct {
            union {
                void* head;    // skiplist
                int fd;        // wal/sst
            };
            union {
                void* pool;    // skiplist
                void* buffer;  // wal/cache
            };
        } storage;
        struct {
            size_t limit;      // memtable
            atomic_size_t used;
        } mem;
        struct {
            uint32_t count;
            void** ptrs;       // shards/sstables
        } array;
    };
} ppdb_base_t;

// 存储层接口
typedef struct {
    ppdb_error_t (*write)(void* impl, const void* data, size_t size);
    ppdb_error_t (*read)(void* impl, void* buf, size_t size);
    ppdb_error_t (*sync)(void* impl);
    ppdb_error_t (*get_stats)(void* impl, ppdb_storage_stats_t* stats);
} ppdb_storage_ops_t;

// 存储层实现
typedef struct {
    ppdb_base_t base;         // 24字节
    ppdb_storage_ops_t* ops;  // 8字节
    ppdb_metrics_t metrics;   // 统计信息
} ppdb_storage_t;

// 容器层接口
typedef struct ppdb_key {
    void* data;
    size_t size;
} ppdb_key_t;

typedef struct ppdb_value {
    void* data;
    size_t size;
} ppdb_value_t;

typedef struct {
    ppdb_error_t (*get)(void* impl, const ppdb_key_t* key, ppdb_value_t* value);
    ppdb_error_t (*put)(void* impl, const ppdb_key_t* key, const ppdb_value_t* value);
    ppdb_error_t (*remove)(void* impl, const ppdb_key_t* key);
    ppdb_error_t (*flush)(void* impl, ppdb_storage_t* dest);
} ppdb_container_ops_t;

// 容器层实现
typedef struct {
    ppdb_base_t base;          // 24字节
    ppdb_container_ops_t* ops; // 8字节
    ppdb_storage_t* storage;   // 8字节
    ppdb_metrics_t metrics;    // 统计信息
} ppdb_container_t;

// KV存储层接口
typedef struct {
    ppdb_error_t (*begin_tx)(void* impl);
    ppdb_error_t (*commit_tx)(void* impl);
    ppdb_error_t (*snapshot)(void* impl, void** snap);
    ppdb_error_t (*compact)(void* impl);
    ppdb_error_t (*get_stats)(void* impl, ppdb_storage_stats_t* stats);
} ppdb_kvstore_ops_t;

// KV存储层实现
typedef struct {
    ppdb_base_t base;          // 24字节
    ppdb_kvstore_ops_t* ops;   // 8字节
    ppdb_container_t* active;  // 8字节
    ppdb_container_t* imm;     // 8字节
    ppdb_storage_t* wal;       // 8字节
    ppdb_storage_t** sst;      // 8字节
    ppdb_metrics_t metrics;    // 统计信息
} ppdb_kvstore_t;

// 公共API函数声明
ppdb_error_t ppdb_open(const char* path, ppdb_kvstore_t** db);
ppdb_error_t ppdb_close(ppdb_kvstore_t* db);
ppdb_error_t ppdb_get(ppdb_kvstore_t* db, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t ppdb_put(ppdb_kvstore_t* db, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t ppdb_remove(ppdb_kvstore_t* db, const ppdb_key_t* key);
ppdb_error_t ppdb_begin_tx(ppdb_kvstore_t* db);
ppdb_error_t ppdb_commit_tx(ppdb_kvstore_t* db);
ppdb_error_t ppdb_snapshot(ppdb_kvstore_t* db, void** snap);
ppdb_error_t ppdb_compact(ppdb_kvstore_t* db);
ppdb_error_t ppdb_get_stats(ppdb_kvstore_t* db, ppdb_storage_stats_t* stats);

// 错误处理函数
const char* ppdb_error_string(ppdb_error_t err);
ppdb_error_t ppdb_system_error(void);

#endif // PPDB_H 