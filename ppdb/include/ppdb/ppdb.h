#ifndef PPDB_H
#define PPDB_H

#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// Memory allocation macros
//-----------------------------------------------------------------------------

#define PPDB_ALIGNMENT 64
#define PPDB_ALIGNED_ALLOC(size) \
    aligned_alloc(PPDB_ALIGNMENT, (((size) + PPDB_ALIGNMENT - 1) / PPDB_ALIGNMENT) * PPDB_ALIGNMENT)
#define PPDB_ALIGNED_FREE(ptr) aligned_free(ptr)

// 对齐宏
#define PPDB_ALIGNED __attribute__((aligned(PPDB_ALIGNMENT)))

//-----------------------------------------------------------------------------
// 常量定义
//-----------------------------------------------------------------------------

#define DEFAULT_MEMTABLE_SIZE (64 * 1024 * 1024)  // 64MB
#define DEFAULT_SHARD_COUNT 16
#define MAX_SKIPLIST_LEVEL 32

//-----------------------------------------------------------------------------
// 前向声明
//-----------------------------------------------------------------------------

struct ppdb_memory_pool;
struct ppdb_advance_ops;
struct ppdb_base;
typedef struct ppdb_memory_pool ppdb_memory_pool_t;
typedef struct ppdb_advance_ops ppdb_advance_ops_t;
typedef struct ppdb_base ppdb_base_t;

//-----------------------------------------------------------------------------
// 存储类型定义
//-----------------------------------------------------------------------------

typedef enum ppdb_type {
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
// 错误码定义
//-----------------------------------------------------------------------------

typedef enum ppdb_error {
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
    PPDB_ERR_SYNC_RETRY_FAILED = -38, // 重试失败
    
    // 新增错误码
    PPDB_ERR_INIT_FAILED = -39,      // 初始化失败
    PPDB_ERR_NOT_INITIALIZED = -40,   // 未初始化
    PPDB_ERR_INVALID_PARAM = -41,     // 无效参数
    PPDB_ERR_NOT_IMPLEMENTED = -42   // 未实现
} ppdb_error_t;

//-----------------------------------------------------------------------------
// 同步原语定义
//-----------------------------------------------------------------------------

typedef enum ppdb_sync_type {
    PPDB_SYNC_MUTEX,     // 互斥锁
    PPDB_SYNC_SPINLOCK,  // 自旋锁
    PPDB_SYNC_RWLOCK     // 读写锁
} ppdb_sync_type_t;

typedef struct ppdb_sync_stats {
    atomic_size_t read_locks;      // 读锁计数
    atomic_size_t write_locks;     // 写锁计数
    atomic_size_t read_timeouts;   // 读锁超时计数
    atomic_size_t write_timeouts;  // 写锁超时计数
    atomic_size_t retries;         // 重试计数
} PPDB_ALIGNED ppdb_sync_stats_t;

typedef struct ppdb_sync_config {
    ppdb_sync_type_t type;
    bool use_lockfree;
    bool enable_ref_count;
    uint32_t max_readers;
    uint32_t backoff_us;
    uint32_t max_retries;
} PPDB_ALIGNED ppdb_sync_config_t;

typedef struct ppdb_sync ppdb_sync_t;

typedef struct ppdb_sync_counter {
    atomic_size_t value;
    ppdb_sync_t* lock;
    #ifdef PPDB_ENABLE_METRICS
    atomic_size_t add_count;
    atomic_size_t sub_count;
    #endif
} PPDB_ALIGNED ppdb_sync_counter_t;

//-----------------------------------------------------------------------------
// 基础类型定义
//-----------------------------------------------------------------------------

typedef struct ppdb_key {
    void* data;
    size_t size;
    ppdb_sync_counter_t ref_count;
} PPDB_ALIGNED ppdb_key_t;

typedef struct ppdb_value {
    void* data;
    size_t size;
    ppdb_sync_counter_t ref_count;
} PPDB_ALIGNED ppdb_value_t;

typedef struct ppdb_node {
    ppdb_sync_t* lock;              
    ppdb_sync_counter_t height;    
    ppdb_sync_counter_t ref_count;  
    ppdb_key_t* key;               
    ppdb_value_t* value;           
    ppdb_sync_counter_t is_deleted;
    ppdb_sync_counter_t is_garbage;
    struct ppdb_node* next[];     // 柔性数组成员
} PPDB_ALIGNED ppdb_node_t;

typedef struct ppdb_metrics {
    ppdb_sync_counter_t get_count;    // 获取计数
    ppdb_sync_counter_t get_hits;     // 命中计数
    ppdb_sync_counter_t put_count;    // 写入计数
    ppdb_sync_counter_t remove_count; // 删除计数
} PPDB_ALIGNED ppdb_metrics_t;

typedef struct ppdb_storage {
    ppdb_node_t* head;
    ppdb_sync_t* lock;
    struct ppdb_memory_pool* pool;
    ppdb_sync_counter_t node_count;
} PPDB_ALIGNED ppdb_storage_t;

typedef struct ppdb_memtable {
    size_t limit;                  // 内存限制
    ppdb_sync_counter_t used;      // 已用内存
    ppdb_sync_t* flush_lock;       // 刷盘锁
} PPDB_ALIGNED ppdb_memtable_t;

typedef struct ppdb_array {
    uint32_t count;               // 分片数量
    ppdb_base_t** ptrs;          // 分片指针数组
} PPDB_ALIGNED ppdb_array_t;

struct ppdb_base {
    ppdb_type_t type;            // 存储类型
    char* path;                  // 存储路径
    ppdb_storage_t storage;      // 存储结构
    ppdb_memtable_t mem;         // 内存表
    ppdb_array_t array;          // 分片数组
    ppdb_metrics_t metrics;      // 统计信息
    ppdb_advance_ops_t* advance; // 高级功能接口
} PPDB_ALIGNED;

//-----------------------------------------------------------------------------
// 函数声明
//-----------------------------------------------------------------------------

// 计数器操作
ppdb_error_t ppdb_sync_counter_init(ppdb_sync_counter_t* counter, size_t initial_value);
void ppdb_sync_counter_destroy(ppdb_sync_counter_t* counter);
size_t ppdb_sync_counter_add(ppdb_sync_counter_t* counter, size_t delta);
size_t ppdb_sync_counter_sub(ppdb_sync_counter_t* counter, size_t delta);
size_t ppdb_sync_counter_load(ppdb_sync_counter_t* counter);
void ppdb_sync_counter_store(ppdb_sync_counter_t* counter, size_t value);
bool ppdb_sync_counter_cas(ppdb_sync_counter_t* counter, size_t expected, size_t desired);

// 其他同步原语函数
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

// 存储接口函数
ppdb_error_t ppdb_create(ppdb_type_t type, ppdb_base_t** base);
void ppdb_destroy(ppdb_base_t* base);
ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key);

void ppdb_ref(ppdb_base_t* base);
void ppdb_unref(ppdb_base_t* base);
ppdb_error_t ppdb_check_type(ppdb_base_t* base, ppdb_type_t type);

#endif // PPDB_H
