#ifndef PPDB_BASE_H_
#define PPDB_BASE_H_

#include <cosmopolitan.h>
//#include "ppdb/common.h"

#ifdef __cplusplus
extern "C" {
#endif

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

// 初始化函数
ppdb_status_t ppdb_init(ppdb_base_t* base, ppdb_type_t type);

// 核心操作函数
ppdb_status_t ppdb_get(void* impl, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_status_t ppdb_put(void* impl, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_status_t ppdb_remove(void* impl, const ppdb_key_t* key);
ppdb_status_t ppdb_clear(void* impl);

// 工具函数
void* ppdb_get_extra(ppdb_node_t* node);
uint32_t ppdb_get_type(const ppdb_base_t* base);
void ppdb_stats_update(ppdb_stats_t* stats, ppdb_status_t status);

#ifdef __cplusplus
}
#endif

#endif // PPDB_BASE_H_
