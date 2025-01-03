#ifndef PPDB_H
#define PPDB_H

#include <cosmopolitan.h>

// 原子类型定义
#ifndef atomic_uint64_t
#define atomic_uint64_t _Atomic(uint64_t)
#endif

#ifndef atomic_size_t
#define atomic_size_t _Atomic(size_t)
#endif

// 常量定义
#define DEFAULT_MEMTABLE_SIZE (64 * 1024 * 1024)  // 64MB
#define DEFAULT_SHARD_COUNT 16
#define MAX_SKIPLIST_LEVEL 32

// 错误码定义
typedef enum {
    PPDB_OK = 0,                    // 成功
    PPDB_ERR_NULL_POINTER = -1,     // 空指针
    PPDB_ERR_OUT_OF_MEMORY = -2,    // 内存不足
    PPDB_ERR_NOT_FOUND = -3,        // 未找到
    PPDB_ERR_ALREADY_EXISTS = -4,   // 已存在
    PPDB_ERR_INVALID_TYPE = -5,     // 无效类型
    PPDB_ERR_LOCK_FAILED = -6,      // 加锁失败
    PPDB_ERR_FULL = -7,            // 存储已满
    PPDB_ERR_NOT_IMPLEMENTED = -8   // 未实现
} ppdb_error_t;

// 类型标记
typedef enum {
    PPDB_TYPE_SKIPLIST = 1,    // 基础跳表
    PPDB_TYPE_MEMTABLE = 2,    // 内存表
    PPDB_TYPE_SHARDED = 4,     // 分片表
    PPDB_TYPE_KVSTORE = 8      // KV存储
} ppdb_type_t;

// 头部信息（4字节）
typedef struct {
    unsigned type : 4;        // 类型（16种）
    unsigned flags : 12;      // 状态标记
    atomic_uint_least16_t refs;  // 引用计数
} ppdb_header_t;

// 统计信息
typedef struct {
    atomic_uint64_t get_count;      // Get操作总数
    atomic_uint64_t get_hits;       // Get命中次数
    atomic_uint64_t put_count;      // Put操作次数
    atomic_uint64_t remove_count;   // Remove操作次数
} ppdb_metrics_t;

// 键值对定义
typedef struct {
    void* data;
    size_t size;
} ppdb_key_t;

typedef struct {
    void* data;
    size_t size;
} ppdb_value_t;

// 跳表节点
typedef struct ppdb_node {
    ppdb_rwlock_t lock;      // 节点锁
    ppdb_key_t* key;         // 键
    ppdb_value_t* value;     // 值
    struct ppdb_node* next[MAX_SKIPLIST_LEVEL];  // 后继指针数组
    uint32_t height;         // 节点高度
} ppdb_node_t;

// 通用存储结构（24字节）
typedef struct {
    ppdb_header_t header;     // 4字节
    ppdb_metrics_t metrics;   // 统计信息
    union {
        struct {
            ppdb_rwlock_t lock;  // 存储锁
            ppdb_node_t* head;   // 头节点
        } storage;
        struct {
            size_t limit;        // 内存限制
            atomic_size_t used;  // 已用内存
        } mem;
        struct {
            uint32_t count;      // 分片数量
            void** ptrs;         // 分片指针数组
        } array;
    };
} ppdb_base_t;

// 公共API函数声明
ppdb_error_t ppdb_create(ppdb_type_t type, ppdb_base_t** base);
void ppdb_destroy(ppdb_base_t* base);
ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key);

// 引用计数管理
void ppdb_ref(ppdb_base_t* base);
void ppdb_unref(ppdb_base_t* base);

// 类型检查
ppdb_error_t ppdb_check_type(ppdb_base_t* base, ppdb_type_t type);

#endif // PPDB_H 