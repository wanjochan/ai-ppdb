#ifndef PPDB_H
#define PPDB_H

#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// 日志系统定义
//-----------------------------------------------------------------------------
typedef enum ppdb_log_level {
    PPDB_LOG_DEBUG = 0,
    PPDB_LOG_INFO,
    PPDB_LOG_WARN,
    PPDB_LOG_ERROR,
    PPDB_LOG_FATAL
} ppdb_log_level_t;

typedef struct ppdb_log_config {
    bool enabled;
    ppdb_log_level_t level;
    const char* log_file;
    int outputs;
} ppdb_log_config_t;

// 日志函数声明
void ppdb_log_init(const ppdb_log_config_t* config);
void ppdb_log_cleanup(void);
void ppdb_log(ppdb_log_level_t level, const char* fmt, ...);
void ppdb_debug(const char* fmt, ...);

//-----------------------------------------------------------------------------
// 错误码定义
//-----------------------------------------------------------------------------
typedef enum ppdb_error {
    PPDB_OK = 0,                    // 成功
    PPDB_ERR_NULL_POINTER,          // 空指针错误
    PPDB_ERR_OUT_OF_MEMORY,         // 内存不足
    PPDB_ERR_INVALID_ARGUMENT,      // 无效参数
    PPDB_ERR_INVALID_TYPE,          // 无效类型
    PPDB_ERR_NOT_FOUND,             // 未找到
    PPDB_ERR_ALREADY_EXISTS,        // 已存在
    PPDB_ERR_BUSY,                  // 资源忙
    PPDB_ERR_TIMEOUT,               // 超时
    PPDB_ERR_LOCK_FAILED,           // 加锁失败
    PPDB_ERR_UNLOCK_FAILED,         // 解锁失败
    PPDB_ERR_INTERNAL,              // 内部错误
    PPDB_ERR_NOT_IMPLEMENTED,       // 未实现
    PPDB_ERR_INVALID_STATE,         // 无效状态
    PPDB_ERR_INVALID_CONFIG,        // 无效配置
    PPDB_ERR_INVALID_SIZE,          // 无效大小
    PPDB_ERR_INVALID_OPERATION,     // 无效操作
    PPDB_ERR_OPERATION_FAILED,      // 操作失败
} ppdb_error_t;

//-----------------------------------------------------------------------------
// 常量定义
//-----------------------------------------------------------------------------
#define MAX_SKIPLIST_LEVEL 32          // 跳表最大层数
#define DEFAULT_MEMTABLE_SIZE (64*1024*1024)  // 默认内存表大小：64MB
#define DEFAULT_SHARD_COUNT 16         // 默认分片数量
#define MAX_KEY_SIZE (16*1024)         // 最大键大小：16KB
#define MAX_VALUE_SIZE (64*1024)       // 最大值大小：64KB
#define MAX_PATH_LENGTH 1024           // 最大路径长度

//-----------------------------------------------------------------------------
// 内存管理
//-----------------------------------------------------------------------------
#define PPDB_ALIGNMENT 64

#define PPDB_ALIGNED __attribute__((aligned(PPDB_ALIGNMENT)))
#define PPDB_CACHELINE_ALIGNED __attribute__((aligned(64)))

#define PPDB_ALIGNED_ALLOC(size) \
    aligned_alloc(PPDB_ALIGNMENT, (((size) + PPDB_ALIGNMENT - 1) / PPDB_ALIGNMENT) * PPDB_ALIGNMENT)
#define PPDB_ALIGNED_FREE(ptr) free(ptr)

void* aligned_alloc(size_t alignment, size_t size);
void aligned_free(void* ptr);

//-----------------------------------------------------------------------------
// 同步原语类型定义
//-----------------------------------------------------------------------------
typedef enum ppdb_sync_type {
    PPDB_SYNC_MUTEX,     // 互斥锁
    PPDB_SYNC_SPINLOCK,  // 自旋锁
    PPDB_SYNC_RWLOCK     // 读写锁
} ppdb_sync_type_t;

// 同步计数器
typedef struct ppdb_sync_counter {
    atomic_size_t value PPDB_CACHELINE_ALIGNED;
    struct ppdb_sync* lock;
    #ifdef PPDB_ENABLE_METRICS
    atomic_size_t add_count PPDB_CACHELINE_ALIGNED;
    atomic_size_t sub_count PPDB_CACHELINE_ALIGNED;
    __thread size_t local_add_count;
    __thread size_t local_sub_count;
    #endif
} PPDB_CACHELINE_ALIGNED ppdb_sync_counter_t;

// 同步统计
typedef struct ppdb_sync_stats {
    ppdb_sync_counter_t read_locks;      // 读锁计数
    ppdb_sync_counter_t write_locks;     // 写锁计数
    ppdb_sync_counter_t read_timeouts;   // 读锁超时计数
    ppdb_sync_counter_t write_timeouts;  // 写锁超时计数
    ppdb_sync_counter_t retries;         // 重试计数
} PPDB_ALIGNED ppdb_sync_stats_t;

// 同步配置
typedef struct ppdb_sync_config {
    ppdb_sync_type_t type;
    bool use_lockfree;
    bool enable_ref_count;
    uint32_t max_readers;
    uint32_t backoff_us;
    uint32_t max_retries;
} ppdb_sync_config_t;

// 同步原语
typedef struct ppdb_sync {
    ppdb_sync_config_t config;
    ppdb_sync_stats_t stats;
    pthread_mutex_t mutex;       // 互斥锁
    atomic_flag spinlock;        // 自旋锁
    pthread_rwlock_t rwlock;     // 读写锁
} PPDB_ALIGNED ppdb_sync_t;

//-----------------------------------------------------------------------------
// 存储类型定义
//-----------------------------------------------------------------------------
typedef enum ppdb_type {
    // 基础存储类型 (0x00-0xFF)
    PPDB_TYPE_SKIPLIST = 0x01,    // 跳表:  0001
    PPDB_TYPE_BTREE    = 0x02,    // B树:   0010
    PPDB_TYPE_LSM      = 0x04,    // LSM:   0100
    PPDB_TYPE_HASH     = 0x08,    // 哈希:  1000
    
    // 保留区域，为未来扩展预留空间 (0x10-0xFF)
    PPDB_TYPE_RESERVED = 0xFF
} ppdb_type_t;

typedef enum ppdb_layer {
    // 存储层次 (0x100-0xF00)
    PPDB_LAYER_MEMTABLE = 0x100,  // 内存表层
    PPDB_LAYER_KVSTORE  = 0x200   // 持久化层
} ppdb_layer_t;

typedef enum ppdb_feature {
    // 存储特性 (0x1000-0xF000)
    PPDB_FEAT_SHARDED   = 0x1000  // 分片特性
} ppdb_feature_t;

// 存储类型组合
#define PPDB_TYPE_MEMTABLE  (PPDB_TYPE_SKIPLIST | PPDB_LAYER_MEMTABLE)
#define PPDB_TYPE_SHARDED   PPDB_FEAT_SHARDED
#define PPDB_TYPE_KVSTORE   (PPDB_TYPE_LSM | PPDB_LAYER_KVSTORE)

#define PPDB_MEMKV_DEFAULT  (PPDB_TYPE_SKIPLIST | PPDB_LAYER_MEMTABLE)
#define PPDB_MEMKV_SHARDED  (PPDB_TYPE_SKIPLIST | PPDB_LAYER_MEMTABLE | PPDB_FEAT_SHARDED)
#define PPDB_DISKV_DEFAULT  (PPDB_TYPE_LSM | PPDB_LAYER_KVSTORE)
#define PPDB_DISKV_SHARDED  (PPDB_TYPE_LSM | PPDB_LAYER_KVSTORE | PPDB_FEAT_SHARDED)

//-----------------------------------------------------------------------------
// 基础数据结构
//-----------------------------------------------------------------------------
// 键值对
typedef struct ppdb_key {
    void* data;
    size_t size;
    ppdb_sync_counter_t ref_count;  // 添加引用计数
} PPDB_ALIGNED ppdb_key_t;

typedef struct ppdb_value {
    void* data;
    size_t size;
    ppdb_sync_counter_t ref_count;  // 添加引用计数
} PPDB_ALIGNED ppdb_value_t;

// 配置
typedef struct ppdb_config {
    ppdb_type_t type;
    ppdb_layer_t layer;
    ppdb_feature_t feature;
    size_t memory_limit;
    uint32_t shard_count;
    uint32_t max_level;
    size_t memtable_size;
    bool use_lockfree;      // 添加无锁标志
    size_t max_key_size;    // 添加最大键大小
    size_t max_value_size;  // 添加最大值大小
} PPDB_ALIGNED ppdb_config_t;

// 前向声明
typedef struct ppdb_base ppdb_base_t;
typedef ppdb_base_t ppdb_t;

//-----------------------------------------------------------------------------
// 公共API接口
//-----------------------------------------------------------------------------
// 数据库操作
ppdb_error_t ppdb_create(ppdb_base_t** base, const ppdb_config_t* config);
void ppdb_destroy(ppdb_base_t* base);

// 基本操作
ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key);

// 存储管理
ppdb_error_t ppdb_storage_sync(ppdb_base_t* base);
ppdb_error_t ppdb_storage_flush(ppdb_base_t* base);
ppdb_error_t ppdb_storage_compact(ppdb_base_t* base);

// 错误处理
const char* ppdb_strerror(ppdb_error_t err);

//-----------------------------------------------------------------------------
// 同步原语API
//-----------------------------------------------------------------------------
// 同步原语操作
ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, ppdb_sync_config_t* config);
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync);

// 基本锁操作
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync);

// 读写锁操作
ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_read_lock(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_write_lock(ppdb_sync_t* sync);

// 同步计数器操作
ppdb_error_t ppdb_sync_counter_init(ppdb_sync_counter_t* counter, size_t initial_value);
size_t ppdb_sync_counter_cleanup(ppdb_sync_counter_t* counter);
size_t ppdb_sync_counter_inc(ppdb_sync_counter_t* counter);
size_t ppdb_sync_counter_dec(ppdb_sync_counter_t* counter);
size_t ppdb_sync_counter_get(ppdb_sync_counter_t* counter);
size_t ppdb_sync_counter_set(ppdb_sync_counter_t* counter, size_t value);
size_t ppdb_sync_counter_add(ppdb_sync_counter_t* counter, size_t value);
size_t ppdb_sync_counter_sub(ppdb_sync_counter_t* counter, size_t value);
size_t ppdb_sync_counter_load(ppdb_sync_counter_t* counter);
size_t ppdb_sync_counter_store(ppdb_sync_counter_t* counter, size_t value);
bool ppdb_sync_counter_cas(ppdb_sync_counter_t* counter, size_t expected, size_t desired);

#endif // PPDB_H
