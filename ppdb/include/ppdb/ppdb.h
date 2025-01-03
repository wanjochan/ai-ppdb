#ifndef PPDB_H
#define PPDB_H

#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// 原子类型定义
//-----------------------------------------------------------------------------

#ifndef atomic_uint64_t
#define atomic_uint64_t _Atomic(uint64_t)
#endif

#ifndef atomic_size_t
#define atomic_size_t _Atomic(size_t)
#endif

//-----------------------------------------------------------------------------
// 常量定义
//-----------------------------------------------------------------------------

#define DEFAULT_MEMTABLE_SIZE (64 * 1024 * 1024)  // 64MB
#define DEFAULT_SHARD_COUNT 16
#define MAX_SKIPLIST_LEVEL 32

//-----------------------------------------------------------------------------
// 错误码定义
//-----------------------------------------------------------------------------

typedef enum {
    // 通用错误码
    PPDB_OK = 0,                    // 成功
    PPDB_ERR_NULL_POINTER = -1,     // 空指针
    PPDB_ERR_OUT_OF_MEMORY = -2,    // 内存不足
    PPDB_ERR_NOT_FOUND = -3,        // 未找到
    PPDB_ERR_ALREADY_EXISTS = -4,   // 已存在
    PPDB_ERR_INVALID_TYPE = -5,     // 无效类型
    PPDB_ERR_INVALID_STATE = -6,    // 无效状态
    PPDB_ERR_INTERNAL = -7,         // 内部错误
    PPDB_ERR_NOT_SUPPORTED = -8,    // 不支持
    
    // 存储相关错误码
    PPDB_ERR_FULL = -16,           // 存储已满
    PPDB_ERR_EMPTY = -17,          // 存储为空
    PPDB_ERR_CORRUPTED = -18,      // 数据损坏
    PPDB_ERR_IO = -19,             // IO错误
    
    // 同步相关错误码
    PPDB_ERR_BUSY = -32,           // 资源忙
    PPDB_ERR_TIMEOUT = -33,        // 超时
    PPDB_ERR_LOCK_FAILED = -34,    // 加锁失败
    PPDB_ERR_UNLOCK_FAILED = -35,  // 解锁失败
    PPDB_ERR_TOO_MANY_READERS = -36, // 读者过多
    PPDB_ERR_RETRY = -37,          // 需要重试
    PPDB_ERR_SYNC_RETRY_FAILED = -38 // 重试失败
} ppdb_error_t;

//-----------------------------------------------------------------------------
// 前向声明
//-----------------------------------------------------------------------------

typedef struct ppdb_sync ppdb_sync_t;
typedef struct ppdb_advance_ops ppdb_advance_ops_t;

//-----------------------------------------------------------------------------
// 同步原语定义
//-----------------------------------------------------------------------------

typedef enum {
    PPDB_SYNC_MUTEX,     // 互斥锁
    PPDB_SYNC_SPINLOCK,  // 自旋锁
    PPDB_SYNC_RWLOCK     // 读写锁
} ppdb_sync_type_t;

typedef struct ppdb_sync_config {
    ppdb_sync_type_t type;
    bool use_lockfree;
    bool enable_ref_count;
    uint32_t max_readers;
    uint32_t backoff_us;
    uint32_t max_retries;
} ppdb_sync_config_t;

typedef struct ppdb_sync_stats {
    atomic_uint_least64_t read_locks;
    atomic_uint_least64_t write_locks;
    atomic_uint_least64_t read_timeouts;
    atomic_uint_least64_t write_timeouts;
    atomic_uint_least64_t contentions;
} ppdb_sync_stats_t;

//-----------------------------------------------------------------------------
// 基础类型定义
//-----------------------------------------------------------------------------

// 键值对定义
typedef struct ppdb_key {
    void* data;
    size_t size;
} ppdb_key_t;

typedef struct ppdb_value {
    void* data;
    size_t size;
} ppdb_value_t;

// 存储节点结构
typedef struct ppdb_node {
    ppdb_sync_t* lock;       // 节点锁
    ppdb_key_t* key;         // 键
    ppdb_value_t* value;     // 值
    struct ppdb_node* next[MAX_SKIPLIST_LEVEL];  // 后继指针数组
    uint32_t height;         // 节点高度
} ppdb_node_t;

// 存储结构
typedef struct ppdb_storage {
    ppdb_node_t* head;
    ppdb_sync_t* lock;
} ppdb_storage_t;

// 统计计数器
typedef struct ppdb_metrics_counters {
    atomic_uint64_t get_count;      // Get操作总数
    atomic_uint64_t get_hits;       // Get命中次数
    atomic_uint64_t put_count;      // Put操作次数
    atomic_uint64_t remove_count;   // Remove操作次数
} ppdb_metrics_counters_t;

// 基础结构
typedef struct ppdb_base {
    char* path;
    ppdb_storage_t storage;
    ppdb_metrics_counters_t metrics;
    ppdb_advance_ops_t* advance;  // 高级功能接口
} ppdb_base_t;

//-----------------------------------------------------------------------------
// 存储类型定义
//-----------------------------------------------------------------------------

typedef enum {
    // 基础数据结构 (0x01-0x0F)
    PPDB_TYPE_SKIPLIST = 0x01,    // 跳表
    PPDB_TYPE_BTREE = 0x02,       // B树
    PPDB_TYPE_HASH = 0x03,        // 哈希表
    
    // 内存存储 (0x10-0x1F)
    PPDB_TYPE_MEMTABLE = 0x10,    // 内存表（基于跳表）
    PPDB_TYPE_MEMKV = 0x11,       // 内存KV（分片存储）
    
    // 持久化存储 (0x20-0x2F)
    PPDB_TYPE_WAL = 0x20,         // 预写日志
    PPDB_TYPE_SSTABLE = 0x21,     // 有序字符串表
    PPDB_TYPE_DISKKV = 0x22,      // 磁盘KV存储
    
    // 组合类型 (0x30-0x3F)
    PPDB_TYPE_SHARDED = 0x30,     // 分片存储
    PPDB_TYPE_KVSTORE = 0x31      // KV存储（可以是内存或磁盘）
} ppdb_type_t;

//-----------------------------------------------------------------------------
// 错误处理函数
//-----------------------------------------------------------------------------

const char* ppdb_strerror(ppdb_error_t err);
ppdb_error_t ppdb_system_error(int err);

//-----------------------------------------------------------------------------
// 同步原语函数
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, ppdb_sync_config_t* config);
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, ppdb_sync_config_t* config);
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_read_lock_shared(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock_shared(ppdb_sync_t* sync);

//-----------------------------------------------------------------------------
// 存储接口函数
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_create(ppdb_type_t type, ppdb_base_t** base);
void ppdb_destroy(ppdb_base_t* base);
ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key);

void ppdb_ref(ppdb_base_t* base);
void ppdb_unref(ppdb_base_t* base);
ppdb_error_t ppdb_check_type(ppdb_base_t* base, ppdb_type_t type);

#endif // PPDB_H