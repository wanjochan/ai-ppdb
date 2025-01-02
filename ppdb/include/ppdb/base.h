#ifndef PPDB_BASE_H_
#define PPDB_BASE_H_

#include "cosmopolitan.h"

#ifdef __cplusplus
extern "C" {
#endif

// 基础类型定义
typedef struct {
    uint8_t* data;
    size_t size;
} ppdb_key_t;

typedef struct {
    uint8_t* data;
    size_t size;
} ppdb_value_t;

typedef enum {
    PPDB_OK = 0,
    PPDB_ERROR = 1,
    PPDB_NOT_FOUND = 2,
    PPDB_INVALID_ARGUMENT = 3,
    PPDB_NOT_SUPPORTED = 4,
    PPDB_NO_MEMORY = 5
} ppdb_status_t;

// 类型标记
typedef enum {
    PPDB_TYPE_SKIPLIST = 1,    // 基础跳表
    PPDB_TYPE_MEMTABLE = 2,    // 内存表
    PPDB_TYPE_SHARDED = 3,     // 分片存储
    PPDB_TYPE_SSTABLE = 4,     // SSTable
    PPDB_TYPE_KVSTORE = 5      // KV存储
} ppdb_type_t;

// 节点类型定义
typedef struct ppdb_node {
    uint64_t data;         // 键
    void* extra;           // 值
    void* ptr;            // 指向下一个节点
} ppdb_node_t;

// 存储统计
typedef struct {
    uint64_t num_items;
    uint64_t num_bytes;
    uint64_t num_gets;
    uint64_t num_puts;
    uint64_t num_deletes;
} ppdb_stats_t;

// 存储结构
typedef struct {
    ppdb_node_t* head;    // 头节点
    void* pool;          // 内存池
    size_t pool_size;    // 内存池大小
    ppdb_stats_t stats;  // 统计信息
} ppdb_storage_t;

// 基础结构
typedef struct {
    ppdb_type_t type;
    ppdb_storage_t storage;
    void* user_data;
} ppdb_base_t;

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
    void* extra;             // 额外数据，8字节
    uint32_t padding;        // 4字节(对齐)
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

// 基础操作接口
typedef struct {
    int (*init)(void* impl);
    int (*destroy)(void* impl);
    int (*get)(void* impl, const ppdb_key_t* key, ppdb_value_t* value);
    int (*put)(void* impl, const ppdb_key_t* key, const ppdb_value_t* value);
    int (*remove)(void* impl, const ppdb_key_t* key);
    int (*clear)(void* impl);
} ppdb_ops_t;

// 统计信息
typedef struct {
    uint64_t get_count;
    uint64_t get_hits;
    uint64_t get_misses;
    uint64_t put_count;
    uint64_t remove_count;
    uint64_t total_size;
} ppdb_stats_t;

// 初始化和销毁
ppdb_status_t ppdb_init(ppdb_base_t* base, ppdb_type_t type);
void ppdb_destroy(ppdb_base_t* base);

// 基本操作
ppdb_status_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_status_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_status_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key);

#ifdef __cplusplus
}
#endif

#endif // PPDB_BASE_H_
