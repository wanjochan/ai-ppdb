#ifndef PPDB_H
#define PPDB_H

#include <cosmopolitan.h>

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
// Memory allocation macros
//-----------------------------------------------------------------------------

#define PPDB_ALIGNMENT 64
#define PPDB_ALIGNED_ALLOC(size) \
    aligned_alloc(PPDB_ALIGNMENT, (((size) + PPDB_ALIGNMENT - 1) / PPDB_ALIGNMENT) * PPDB_ALIGNMENT)
#define PPDB_ALIGNED_FREE(ptr) aligned_free(ptr)

// 对齐宏
#define PPDB_ALIGNED __attribute__((aligned(PPDB_ALIGNMENT)))

// 内存分配函数
void* aligned_alloc(size_t alignment, size_t size);
void aligned_free(void* ptr);

//-----------------------------------------------------------------------------
// 基础类型定义
//-----------------------------------------------------------------------------

// 错误码定义
typedef enum ppdb_error {
    PPDB_OK = 0,                  // 成功
    PPDB_ERR_NULL_POINTER,        // 空指针错误
    PPDB_ERR_OUT_OF_MEMORY,       // 内存不足
    PPDB_ERR_ALREADY_EXISTS,      // 已存在
    PPDB_ERR_NOT_FOUND,          // 未找到
    PPDB_ERR_BUSY,               // 资源忙
    PPDB_ERR_IO,                 // IO错误
    PPDB_ERR_INTERNAL,           // 内部错误
    PPDB_ERR_INVALID_TYPE,       // 无效类型
    PPDB_ERR_LOCK_FAILED,        // 加锁失败
    PPDB_ERR_UNLOCK_FAILED,      // 解锁失败
    PPDB_ERR_NOT_INITIALIZED,    // 未初始化
    PPDB_ERR_INVALID_ARGUMENT,   // 无效参数
    PPDB_ERR_TIMEOUT,            // 超时
    PPDB_ERR_FULL,              // 已满
} ppdb_error_t;

// 存储类型定义
typedef enum ppdb_type {
    PPDB_TYPE_SKIPLIST,          // 跳表
    PPDB_TYPE_BTREE,             // B树
    PPDB_TYPE_LSM,               // LSM树
    PPDB_TYPE_HASH,              // 哈希表
    PPDB_TYPE_MEMTABLE,          // 内存表
    PPDB_TYPE_SHARDED,           // 分片存储
    PPDB_TYPE_KVSTORE,           // KV存储
} ppdb_type_t;

//-----------------------------------------------------------------------------
// 同步原语定义
//-----------------------------------------------------------------------------

typedef enum ppdb_sync_type {
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
} PPDB_ALIGNED ppdb_sync_config_t;

// 前向声明
struct ppdb_sync;

typedef struct ppdb_sync_counter {
    atomic_size_t value;
    struct ppdb_sync* lock;
    #ifdef PPDB_ENABLE_METRICS
    atomic_size_t add_count;
    atomic_size_t sub_count;
    #endif
} PPDB_ALIGNED ppdb_sync_counter_t;

typedef struct ppdb_sync_stats {
    ppdb_sync_counter_t read_locks;      // 读锁计数
    ppdb_sync_counter_t write_locks;     // 写锁计数
    ppdb_sync_counter_t read_timeouts;   // 读锁超时计数
    ppdb_sync_counter_t write_timeouts;  // 写锁超时计数
    ppdb_sync_counter_t retries;         // 重试计数
} PPDB_ALIGNED ppdb_sync_stats_t;

typedef struct ppdb_sync {
    ppdb_sync_config_t config;
    ppdb_sync_stats_t stats;
    pthread_mutex_t mutex;       // 互斥锁
    atomic_flag spinlock;        // 自旋锁
    pthread_rwlock_t rwlock;     // 读写锁
} PPDB_ALIGNED ppdb_sync_t;

//-----------------------------------------------------------------------------
// 存储结构定义
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

//-----------------------------------------------------------------------------
// 内存池定义
//-----------------------------------------------------------------------------

typedef struct ppdb_memory_block {
    void* data;
    size_t size;
    size_t used;
    struct ppdb_memory_block* next;
} ppdb_memory_block_t;

typedef struct ppdb_memory_pool {
    ppdb_memory_block_t* current;
    ppdb_memory_block_t* blocks;
    size_t block_size;
    ppdb_sync_t* lock;
    ppdb_sync_counter_t total_size;
    ppdb_sync_counter_t used_size;
} ppdb_memory_pool_t;

//-----------------------------------------------------------------------------
// 存储结构定义
//-----------------------------------------------------------------------------

typedef struct ppdb_storage {
    ppdb_node_t* head;
    ppdb_sync_t* lock;
    ppdb_memory_pool_t* pool;
    ppdb_sync_counter_t node_count;
} PPDB_ALIGNED ppdb_storage_t;

typedef struct ppdb_memtable {
    size_t limit;                  // 内存限制
    ppdb_sync_counter_t used;      // 已用内存
    ppdb_sync_t* flush_lock;       // 刷盘锁
} PPDB_ALIGNED ppdb_memtable_t;

typedef struct ppdb_array {
    uint32_t count;               // 分片数量
    struct ppdb_base** ptrs;      // 分片指针数组
} PPDB_ALIGNED ppdb_array_t;

// 前向声明
typedef struct ppdb_base ppdb_base_t;

//-----------------------------------------------------------------------------
// 高级操作定义
//-----------------------------------------------------------------------------

typedef struct ppdb_advance_ops {
    ppdb_error_t (*compact)(ppdb_base_t* base);
    ppdb_error_t (*sync)(ppdb_base_t* base);
    ppdb_error_t (*backup)(ppdb_base_t* base, const char* path);
    ppdb_error_t (*restore)(ppdb_base_t* base, const char* path);
    ppdb_error_t (*iterator)(ppdb_base_t* base, void** iter);
    ppdb_error_t (*next)(void* iter, ppdb_key_t* key, ppdb_value_t* value);
    void (*iterator_destroy)(void* iter);
} ppdb_advance_ops_t;

typedef struct ppdb_config {
    ppdb_type_t type;                // 存储类型
    const char* path;                // 存储路径
    size_t memtable_size;           // 内存表大小
    uint32_t shard_count;           // 分片数量
    bool use_lockfree;              // 是否使用无锁模式
} PPDB_ALIGNED ppdb_config_t;

struct ppdb_base {
    ppdb_type_t type;            // 存储类型
    char* path;                  // 存储路径
    ppdb_storage_t storage;      // 存储结构
    ppdb_memtable_t mem;         // 内存表
    ppdb_array_t array;          // 分片数组
    ppdb_metrics_t metrics;      // 统计信息
    ppdb_advance_ops_t* advance; // 高级功能接口
    ppdb_config_t config;        // 存储配置
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

// 同步原语函数
ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, ppdb_sync_config_t* config);
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, ppdb_sync_config_t* config);
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_read_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_write_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync);

// 内存池操作
ppdb_error_t ppdb_memory_pool_create(ppdb_memory_pool_t** pool, size_t block_size);
void ppdb_memory_pool_destroy(ppdb_memory_pool_t* pool);
void* ppdb_memory_pool_alloc(ppdb_memory_pool_t* pool, size_t size);
void ppdb_memory_pool_free(ppdb_memory_pool_t* pool, void* ptr);

// 存储接口函数
ppdb_error_t ppdb_create(ppdb_type_t type, ppdb_base_t** base);
void ppdb_destroy(ppdb_base_t* base);
ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key);

// 文件系统操作
ppdb_error_t ppdb_fs_init(const char* path);
ppdb_error_t ppdb_fs_cleanup(const char* path);
ppdb_error_t ppdb_fs_write(const char* path, const void* data, size_t size);
ppdb_error_t ppdb_fs_read(const char* path, void* data, size_t size, size_t* bytes_read);
ppdb_error_t ppdb_fs_append(const char* path, const void* data, size_t size);
bool ppdb_fs_exists(const char* path);
bool ppdb_fs_is_file(const char* path);
bool ppdb_fs_is_dir(const char* path);

// 存储操作
ppdb_error_t ppdb_storage_sync(ppdb_base_t* base);
ppdb_error_t ppdb_storage_flush(ppdb_base_t* base);
ppdb_error_t ppdb_storage_compact(ppdb_base_t* base);
ppdb_error_t ppdb_storage_get_stats(ppdb_base_t* base, ppdb_metrics_t* stats);

// 存储创建和销毁
ppdb_error_t ppdb_skiplist_create(ppdb_base_t* base, const ppdb_config_t* config);
ppdb_error_t ppdb_memtable_create(ppdb_base_t* base, const ppdb_config_t* config);
ppdb_error_t ppdb_sharded_create(ppdb_base_t* base, const ppdb_config_t* config);
ppdb_error_t ppdb_kvstore_create(ppdb_base_t* base, const ppdb_config_t* config);
void ppdb_skiplist_destroy(ppdb_base_t* base);
void ppdb_memtable_destroy(ppdb_base_t* base);
void ppdb_sharded_destroy(ppdb_base_t* base);
void ppdb_kvstore_destroy(ppdb_base_t* base);

// 错误处理
const char* ppdb_strerror(ppdb_error_t err);
ppdb_error_t ppdb_system_error(int err);

#endif // PPDB_H
