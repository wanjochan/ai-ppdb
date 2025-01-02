#ifndef PPDB_BASE_H_
#define PPDB_BASE_H_

//这是根据 NEXT4.md 启动的优化分支

#include "cosmopolitan.h"
#include "ppdb/ppdb_error.h"  // 引入错误码定义
#include "ppdb/ppdb_types.h"  // 引入统计信息定义

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

// 类型标记
typedef enum {
    PPDB_TYPE_SKIPLIST = 1,    // 基础跳表
    PPDB_TYPE_MEMTABLE = 2,    // 内存表
    PPDB_TYPE_SHARDED = 4,     // 分片存储
    PPDB_TYPE_SSTABLE = 8,     // SSTable
    PPDB_TYPE_KVSTORE = 128    // KV存储
} ppdb_type_t;

// 头部信息（4字节）
typedef struct {
    unsigned type : 4;        // 类型（16种）
    unsigned flags : 12;      // 状态标记
    unsigned refs : 16;       // 引用计数
} ppdb_header_t;

// 节点类型定义
typedef struct ppdb_node {
    ppdb_header_t header;     // 4字节
    union {
        void* ptr;            // 通用指针
        uint64_t data;        // 内联数据
    };                        // 8字节
    void* extra;             // 额外数据，8字节
    uint32_t padding;        // 4字节(对齐)
} ppdb_node_t;

// 存储结构
typedef struct {
    ppdb_node_t* head;    // 头节点
    void* pool;          // 内存池
    size_t pool_size;    // 内存池大小
    ppdb_metrics_t metrics;  // 使用统一的统计信息结构
} ppdb_storage_t;

// 基础结构
typedef struct ppdb_base {
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
    ppdb_metrics_t metrics;  // 添加统计信息
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

// 初始化和销毁
ppdb_error_t ppdb_init(ppdb_base_t* base, ppdb_type_t type);
void ppdb_destroy(ppdb_base_t* base);

// 基本操作
ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key);

#ifdef __cplusplus
}
#endif

#endif // PPDB_BASE_H_
