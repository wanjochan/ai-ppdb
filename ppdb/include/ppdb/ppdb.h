#ifndef PPDB_H
#define PPDB_H

#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// 日志系统定义
//-----------------------------------------------------------------------------

// 日志级别
typedef enum ppdb_log_level {
    PPDB_LOG_DEBUG = 0,
    PPDB_LOG_INFO,
    PPDB_LOG_WARN,
    PPDB_LOG_ERROR,
    PPDB_LOG_FATAL
} ppdb_log_level_t;

// 日志配置
typedef struct ppdb_log_config {
    bool enabled;
    ppdb_log_level_t level;
    const char* log_file;
    int outputs;
} ppdb_log_config_t;

// 日志函数
void ppdb_log_init(const ppdb_log_config_t* config);
void ppdb_log_cleanup(void);
void ppdb_log(ppdb_log_level_t level, const char* fmt, ...);
void ppdb_debug(const char* fmt, ...);

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
#define PPDB_CACHELINE_ALIGNED __attribute__((aligned(64)))

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
    PPDB_ERR_INVALID_STATE,      // 无效状态
    PPDB_ERR_LOCK_FAILED,        // 加锁失败
    PPDB_ERR_UNLOCK_FAILED,      // 解锁失败
    PPDB_ERR_NOT_INITIALIZED,    // 未初始化
    PPDB_ERR_INVALID_ARGUMENT,   // 无效参数
    PPDB_ERR_TIMEOUT,            // 超时
    PPDB_ERR_FULL,              // 已满
    PPDB_ERR_NOT_IMPLEMENTED,   // 未实现
    PPDB_ERR_ITERATOR_INVALID,  // 迭代器无效
    PPDB_ERR_ITERATOR_END,      // 迭代器到达末尾
} ppdb_error_t;

// 存储类型定义
typedef enum ppdb_type {
    // 基础存储类型 (0x00-0xFF)
    PPDB_TYPE_SKIPLIST = 0x01,    // 跳表:  0001
    PPDB_TYPE_BTREE    = 0x02,    // B树:   0010
    PPDB_TYPE_LSM      = 0x04,    // LSM:   0100
    PPDB_TYPE_HASH     = 0x08,    // 哈希:  1000
    
    // 保留区域，为未来扩展预留空间 (0x10-0xFF)
    PPDB_TYPE_RESERVED = 0xFF,
    
    // 存储层次 (0x100-0xF00)
    PPDB_LAYER_MEMTABLE = 0x100,  // 内存表层
    PPDB_LAYER_KVSTORE  = 0x200,  // 持久化层
    
    // 存储特性 (0x1000-0xF000)
    PPDB_FEAT_SHARDED   = 0x1000  // 分片特性
} ppdb_type_t;

// 向后兼容的类型定义
#define PPDB_TYPE_MEMTABLE  (PPDB_TYPE_SKIPLIST | PPDB_LAYER_MEMTABLE)
#define PPDB_TYPE_SHARDED   PPDB_FEAT_SHARDED
#define PPDB_TYPE_KVSTORE   (PPDB_TYPE_LSM | PPDB_LAYER_KVSTORE)

// 常用组合
#define PPDB_MEMKV_DEFAULT  (PPDB_TYPE_SKIPLIST | PPDB_LAYER_MEMTABLE)
#define PPDB_MEMKV_SHARDED  (PPDB_TYPE_SKIPLIST | PPDB_LAYER_MEMTABLE | PPDB_FEAT_SHARDED)
#define PPDB_DISKV_DEFAULT  (PPDB_TYPE_LSM | PPDB_LAYER_KVSTORE)
#define PPDB_DISKV_SHARDED  (PPDB_TYPE_LSM | PPDB_LAYER_KVSTORE | PPDB_FEAT_SHARDED)

// 类型检查辅助宏
#define PPDB_TYPE_BASE(type)    ((type) & 0xFF)
#define PPDB_TYPE_LAYER(type)   ((type) & 0xF00)
#define PPDB_TYPE_FEATURE(type) ((type) & 0xF000)
#define PPDB_IS_SHARDED(type)   ((type) & PPDB_FEAT_SHARDED)

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
    char padding[40];
} PPDB_CACHELINE_ALIGNED ppdb_sync_config_t;

// 前向声明
typedef struct ppdb_base ppdb_base_t;
typedef struct ppdb_peer_s ppdb_peer_t;
typedef ppdb_base_t ppdb_t;  // 修改为使用ppdb_base_t作为ppdb_t

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
    ppdb_sync_t* lock;              // lock for node
    ppdb_sync_counter_t height;     // height of node
    ppdb_sync_counter_t ref_count;  // reference count
    ppdb_key_t* key;               // key
    ppdb_value_t* value;           // value
    ppdb_sync_counter_t is_deleted; // deleted flag
    ppdb_sync_counter_t is_garbage; // garbage flag
    struct ppdb_node* next[];      // next pointers
} PPDB_ALIGNED ppdb_node_t;

typedef struct ppdb_metrics {
    ppdb_sync_counter_t get_count PPDB_CACHELINE_ALIGNED;    // get count
    ppdb_sync_counter_t get_hits PPDB_CACHELINE_ALIGNED;     // hit count
    ppdb_sync_counter_t put_count PPDB_CACHELINE_ALIGNED;    // put count
    ppdb_sync_counter_t remove_count PPDB_CACHELINE_ALIGNED; // remove count
    char padding[64];  // 确保整个结构体64字节对齐
} PPDB_CACHELINE_ALIGNED ppdb_metrics_t;

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
    uint32_t shard_count;         // shard count
    ppdb_base_t** shards;         // shard array
} PPDB_ALIGNED ppdb_storage_t;

typedef struct ppdb_memtable {
    size_t limit;                  // memory limit
    ppdb_sync_counter_t used;      // used memory
    ppdb_sync_t* flush_lock;       // flush lock
} PPDB_ALIGNED ppdb_memtable_t;

typedef struct ppdb_array {
    uint32_t count;               // shard count
    struct ppdb_base** ptrs;      // shard pointers
} PPDB_ALIGNED ppdb_array_t;

//-----------------------------------------------------------------------------
// 内部函数声明
//-----------------------------------------------------------------------------

uint32_t get_shard_index(const ppdb_key_t* key, uint32_t shard_count);

//-----------------------------------------------------------------------------
// 
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
    ppdb_type_t type;                // storage type
    const char* path;                // storage path
    size_t memtable_size;           // memtable size
    uint32_t shard_count;           // shard count
    bool use_lockfree;              // use lockfree mode
} PPDB_ALIGNED ppdb_config_t;

struct ppdb_base {
    ppdb_type_t type;            // storage type
    char* path;                  // storage path
    ppdb_storage_t storage;      // storage structure
    ppdb_memtable_t mem;         // memtable
    ppdb_array_t array;          // shard array
    ppdb_metrics_t metrics;      // metrics
    ppdb_advance_ops_t* advance; // advanced operations
    ppdb_config_t config;        // configuration
} PPDB_ALIGNED;

//-----------------------------------------------------------------------------
// 存储操作函数声明
//-----------------------------------------------------------------------------

// 创建存储实例
ppdb_error_t ppdb_create(ppdb_base_t** base, const ppdb_config_t* config);

// 销毁存储实例
void ppdb_destroy(ppdb_base_t* base);

// 基本操作
ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key);

// 迭代器操作
ppdb_error_t ppdb_iterator_init(ppdb_base_t* base, void** iter);
ppdb_error_t ppdb_iterator_next(void* iter, ppdb_key_t* key, ppdb_value_t* value);
void ppdb_iterator_destroy(void* iter);

// 高级操作
ppdb_error_t ppdb_storage_sync(ppdb_base_t* base);
ppdb_error_t ppdb_storage_flush(ppdb_base_t* base);
ppdb_error_t ppdb_storage_compact(ppdb_base_t* base);
ppdb_error_t ppdb_storage_get_stats(ppdb_base_t* base, ppdb_metrics_t* stats);

// 
ppdb_error_t ppdb_sync_counter_init(ppdb_sync_counter_t* counter, size_t initial_value);
void ppdb_sync_counter_destroy(ppdb_sync_counter_t* counter);
size_t ppdb_sync_counter_add(ppdb_sync_counter_t* counter, size_t delta);
size_t ppdb_sync_counter_sub(ppdb_sync_counter_t* counter, size_t delta);
size_t ppdb_sync_counter_load(ppdb_sync_counter_t* counter);
void ppdb_sync_counter_store(ppdb_sync_counter_t* counter, size_t value);
bool ppdb_sync_counter_cas(ppdb_sync_counter_t* counter, size_t expected, size_t desired);

// 
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

// 
ppdb_error_t ppdb_memory_pool_create(ppdb_memory_pool_t** pool, size_t block_size);
void ppdb_memory_pool_destroy(ppdb_memory_pool_t* pool);
void* ppdb_memory_pool_alloc(ppdb_memory_pool_t* pool, size_t size);
void ppdb_memory_pool_free(ppdb_memory_pool_t* pool, void* ptr);

// 
ppdb_error_t ppdb_fs_init(const char* path);
ppdb_error_t ppdb_fs_cleanup(const char* path);
ppdb_error_t ppdb_fs_write(const char* path, const void* data, size_t size);
ppdb_error_t ppdb_fs_read(const char* path, void* data, size_t size, size_t* bytes_read);
ppdb_error_t ppdb_fs_append(const char* path, const void* data, size_t size);
bool ppdb_fs_exists(const char* path);
bool ppdb_fs_is_file(const char* path);
bool ppdb_fs_is_dir(const char* path);

// 
ppdb_error_t ppdb_skiplist_create(ppdb_base_t* base, const ppdb_config_t* config);
ppdb_error_t ppdb_memtable_create(ppdb_base_t* base, const ppdb_config_t* config);
ppdb_error_t ppdb_sharded_create(ppdb_base_t* base, const ppdb_config_t* config);
ppdb_error_t ppdb_kvstore_create(ppdb_base_t* base, const ppdb_config_t* config);

// 
const char* ppdb_strerror(ppdb_error_t err);
ppdb_error_t ppdb_system_error(int err);

// 类型检查辅助函数
static inline bool ppdb_type_has_feature(ppdb_type_t type, ppdb_type_t feature) {
    return (type & feature) == feature;
}

static inline ppdb_type_t ppdb_type_base(ppdb_type_t type) {
    return type & 0xFF;  // 获取基础存储类型
}

static inline ppdb_type_t ppdb_type_layer(ppdb_type_t type) {
    return type & 0xF00;  // 获取存储层次
}

static inline bool ppdb_type_is_sharded(ppdb_type_t type) {
    return ppdb_type_has_feature(type, PPDB_FEAT_SHARDED);
}

//==============================================================================
// Peer API
//==============================================================================

// Peer 相关常量
#define PPDB_DEFAULT_PORT 11211
#define PPDB_MAX_COMMAND_LEN 1024
#define PPDB_MAX_VALUE_SIZE (1024 * 1024)  // 1MB

// Peer 角色和状态
typedef enum ppdb_peer_role_e {
    PPDB_PEER_SERVER = 1,
    PPDB_PEER_CLIENT = 2,
    PPDB_PEER_REPLICA = 3,     // 预留
    PPDB_PEER_CLUSTER = 4      // 预留
} ppdb_peer_role_t;

// Peer 错误码
typedef enum ppdb_peer_error_e {
    PPDB_PEER_OK = 0,
    PPDB_PEER_ERROR = -1,
    PPDB_PEER_AUTH_REQUIRED = -2,
    PPDB_PEER_INVALID_COMMAND = -3,
    PPDB_PEER_NETWORK_ERROR = -4
} ppdb_peer_error_t;

// Peer API 函数
ppdb_peer_t* ppdb_peer_create_server(ppdb_t* db, const char* host, int port);
ppdb_peer_t* ppdb_peer_create_client(void);
int ppdb_peer_start(ppdb_peer_t* peer);
void ppdb_peer_stop(ppdb_peer_t* peer);
void ppdb_peer_free(ppdb_peer_t* peer);
int ppdb_peer_connect(ppdb_peer_t* peer, const char* host, int port);
int ppdb_peer_auth(ppdb_peer_t* peer, const char* user, const char* pass);
int ppdb_peer_execute(ppdb_peer_t* peer, const char* cmd);
int ppdb_peer_join(ppdb_peer_t* peer, const char* cluster);
int ppdb_peer_replicate(ppdb_peer_t* peer, const char* master);

/*
//==============================================================================
// Peer API
//==============================================================================

// Peer 相关常量
#define PPDB_DEFAULT_PORT 11211
#define PPDB_MAX_COMMAND_LEN 1024
#define PPDB_MAX_VALUE_SIZE (1024 * 1024)  // 1MB

// Peer 角色和状态
typedef enum ppdb_peer_role_e {
    PPDB_PEER_SERVER = 1,
    PPDB_PEER_CLIENT = 2,
    PPDB_PEER_REPLICA = 3,     // 预留
    PPDB_PEER_CLUSTER = 4      // 预留
} ppdb_peer_role_t;

// Peer 错误码
typedef enum ppdb_peer_error_e {
    PPDB_PEER_OK = 0,
    PPDB_PEER_ERROR = -1,
    PPDB_PEER_AUTH_REQUIRED = -2,
    PPDB_PEER_INVALID_COMMAND = -3,
    PPDB_PEER_NETWORK_ERROR = -4
} ppdb_peer_error_t;

// Peer API 函数
// 创建peer（服务端/客户端）
ppdb_peer_t* ppdb_peer_create_server(ppdb_t* db, const char* host, int port);
ppdb_peer_t* ppdb_peer_create_client(void);

// 基础操作
int ppdb_peer_start(ppdb_peer_t* peer);
void ppdb_peer_stop(ppdb_peer_t* peer);
void ppdb_peer_free(ppdb_peer_t* peer);

// 客户端操作
int ppdb_peer_connect(ppdb_peer_t* peer, const char* host, int port);
int ppdb_peer_auth(ppdb_peer_t* peer, const char* user, const char* pass);
int ppdb_peer_execute(ppdb_peer_t* peer, const char* cmd);

// 为将来扩展预留的分布式接口
int ppdb_peer_join(ppdb_peer_t* peer, const char* cluster);
int ppdb_peer_replicate(ppdb_peer_t* peer, const char* master);
*/
#endif // PPDB_H
