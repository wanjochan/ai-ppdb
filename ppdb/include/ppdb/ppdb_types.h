#ifndef PPDB_TYPES_H
#define PPDB_TYPES_H

#include <cosmopolitan.h>

// 常量定义
#define PPDB_MAX_PATH_SIZE 256
#define PPDB_MAX_KEY_SIZE 1024
#define PPDB_MAX_VALUE_SIZE (1024 * 1024)  // 1MB
#define PPDB_SKIPLIST_MAX_LEVEL 32

// 原子类型定义
typedef _Atomic uint64_t atomic_uint64_t;

// 压缩类型
typedef enum ppdb_compression_type {
    PPDB_COMPRESSION_NONE = 0,  // 无压缩
    PPDB_COMPRESSION_SNAPPY,    // Snappy压缩
    PPDB_COMPRESSION_LZ4,       // LZ4压缩
    PPDB_COMPRESSION_ZSTD       // ZSTD压缩
} ppdb_compression_t;

// 同步类型
typedef enum ppdb_sync_type {
    PPDB_SYNC_MUTEX,      // 互斥锁
    PPDB_SYNC_SPINLOCK,   // 自旋锁
    PPDB_SYNC_RWLOCK,     // 读写锁
    PPDB_SYNC_LOCKFREE    // 无锁
} ppdb_sync_type_t;

// 运行模式
typedef enum ppdb_mode {
    PPDB_MODE_NORMAL = 0,    // 正常模式
    PPDB_MODE_RECOVERY,      // 恢复模式
    PPDB_MODE_READONLY       // 只读模式
} ppdb_mode_t;

// 同步配置
typedef struct ppdb_sync_config {
    ppdb_sync_type_t type;         // 同步类型
    uint32_t spin_count;           // 自旋次数
    bool use_lockfree;             // 是否使用无锁模式
    uint32_t stripe_count;         // 分片数量
    uint32_t backoff_us;           // 退避时间（微秒）
    bool enable_ref_count;         // 是否启用引用计数
} ppdb_sync_config_t;

// 同步原语
typedef struct ppdb_sync {
    ppdb_sync_type_t type;    // 同步类型
    union {
        _Atomic int mutex;     // 互斥锁
        _Atomic int spinlock;  // 自旋锁
        struct {
            _Atomic int readers;  // 读者计数
            _Atomic int writer;   // 写者标志
        } rwlock;              // 读写锁
    };
} ppdb_sync_t;

// 性能指标
typedef struct ppdb_metrics {
    atomic_uint64_t put_count;         // 写入次数
    atomic_uint64_t get_count;         // 读取次数
    atomic_uint64_t delete_count;      // 删除次数
    atomic_uint64_t total_ops;         // 总操作数
    atomic_uint64_t total_latency;     // 总延迟
    atomic_uint64_t total_latency_us;  // 总延迟（微秒）
    atomic_uint64_t max_latency_us;    // 最大延迟（微秒）
    atomic_uint64_t min_latency_us;    // 最小延迟（微秒）
    atomic_uint64_t total_bytes;       // 总字节数
    atomic_uint64_t total_keys;        // 总键数
    atomic_uint64_t total_values;      // 总值数
    atomic_uint64_t bytes_written;     // 写入字节数
    atomic_uint64_t bytes_read;        // 读取字节数
    atomic_uint64_t get_miss_count;    // 读取未命中次数
} ppdb_metrics_t;

// 键值对
typedef struct ppdb_kv_pair {
    void* key;           // 键
    size_t key_size;     // 键大小
    void* value;         // 值
    size_t value_size;   // 值大小
} ppdb_kv_pair_t;

// 内存表类型
typedef enum ppdb_memtable_type {
    PPDB_MEMTABLE_BASIC = 0,    // 基本内存表
    PPDB_MEMTABLE_SHARDED,      // 分片内存表
    PPDB_MEMTABLE_LOCKFREE      // 无锁内存表
} ppdb_memtable_type_t;

// 跳表节点
typedef struct ppdb_skiplist_node {
    void* key;                  // 键
    size_t key_len;            // 键长度
    void* value;               // 值
    size_t value_len;          // 值长度
    int level;                 // 节点层数
    struct ppdb_skiplist_node** next;  // 后继节点数组
} ppdb_skiplist_node_t;

// 跳表比较函数类型
typedef int (*ppdb_compare_func_t)(const void* key1, size_t key1_len,
                                 const void* key2, size_t key2_len);

// 跳表
typedef struct ppdb_skiplist {
    ppdb_skiplist_node_t* head;  // 头节点
    int max_level;               // 最大层数
    size_t size;                 // 节点数量
    size_t memory_usage;         // 内存使用量
    ppdb_sync_t sync;            // 同步原语
    ppdb_compare_func_t compare; // 比较函数
} ppdb_skiplist_t;

// 内存表分片
typedef struct ppdb_memtable_shard {
    ppdb_skiplist_t* skiplist;   // 跳表
    ppdb_sync_t sync;            // 同步原语
    atomic_size_t size;          // 分片大小
} ppdb_memtable_shard_t;

// 基础内存表
typedef struct ppdb_memtable_basic {
    ppdb_skiplist_t* skiplist;   // 跳表
    ppdb_sync_t sync;            // 同步原语
    size_t used;                 // 已使用大小
    size_t size;                 // 总大小
} ppdb_memtable_basic_t;

// 内存表
typedef struct ppdb_memtable {
    ppdb_memtable_type_t type;      // 内存表类型
    size_t size_limit;              // 大小限制
    atomic_size_t current_size;     // 当前大小
    size_t shard_count;             // 分片数量
    union {
        ppdb_memtable_basic_t* basic;     // 基础版本
        ppdb_memtable_shard_t* shards;    // 分片版本
    };
    ppdb_metrics_t metrics;         // 性能指标
    bool is_immutable;              // 是否不可变
} ppdb_memtable_t;

// 跳表迭代器
typedef struct ppdb_skiplist_iterator {
    ppdb_skiplist_t* list;         // 跳表
    ppdb_skiplist_node_t* current; // 当前节点
    bool valid;                    // 是否有效
    ppdb_kv_pair_t current_pair;   // 当前键值对
    ppdb_sync_t sync;              // 同步原语
} ppdb_skiplist_iterator_t;

// 内存表迭代器
typedef struct ppdb_memtable_iterator {
    ppdb_memtable_t* table;        // 内存表
    ppdb_skiplist_iterator_t* it;  // 跳表迭代器
    bool valid;                    // 是否有效
    ppdb_kv_pair_t current_pair;   // 当前键值对
} ppdb_memtable_iterator_t;

#endif // PPDB_TYPES_H 