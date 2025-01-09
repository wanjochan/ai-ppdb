/*
 * base.h - Base Layer Definitions
 */

#ifndef PPDB_BASE_H
#define PPDB_BASE_H

#include <cosmopolitan.h>

// Constants
#define PPDB_MAX_ERROR_MESSAGE 256
#define PPDB_MAX_SKIPLIST_LEVEL 32

// IO manager constants
#define PPDB_IO_DEFAULT_QUEUE_SIZE 1024
#define PPDB_IO_MIN_THREADS 2
#define PPDB_IO_MAX_THREADS 64
#define PPDB_IO_DEFAULT_THREADS 4
#define PPDB_IO_QUEUE_PRIORITIES 4

// Event system constants
#define PPDB_EVENT_MAX_EVENTS 64
#define PPDB_EVENT_MAX_FILTERS 16
#define PPDB_EVENT_READ  0x01
#define PPDB_EVENT_WRITE 0x02
#define PPDB_EVENT_ERROR 0x04

// Timer wheel configuration
#define PPDB_TIMER_WHEEL_BITS 8
#define PPDB_TIMER_WHEEL_SIZE (1 << PPDB_TIMER_WHEEL_BITS)
#define PPDB_TIMER_WHEEL_MASK (PPDB_TIMER_WHEEL_SIZE - 1)
#define PPDB_TIMER_WHEEL_COUNT 4

// Timer priorities
#define PPDB_TIMER_PRIORITY_HIGH 0
#define PPDB_TIMER_PRIORITY_NORMAL 1
#define PPDB_TIMER_PRIORITY_LOW 2

// Log levels
#define PPDB_LOG_ERROR 0
#define PPDB_LOG_WARN  1
#define PPDB_LOG_INFO  2
#define PPDB_LOG_DEBUG 3

// Common status codes
#define PPDB_OK 0

// Error codes (4000-4199)
#define PPDB_ERR_START    4000
#define PPDB_ERR_PARAM    4001
#define PPDB_ERR_MEMORY   4002
#define PPDB_ERR_SYSTEM   4003
#define PPDB_ERR_NOT_FOUND 4004
#define PPDB_ERR_EXISTS   4005
#define PPDB_ERR_TIMEOUT  4006
#define PPDB_ERR_BUSY     4007
#define PPDB_ERR_FULL     4008
#define PPDB_ERR_EMPTY    4009
#define PPDB_ERR_IO       4010
#define PPDB_ERR_INTERNAL 4011
#define PPDB_ERR_THREAD   4012
#define PPDB_ERR_MUTEX    4013
#define PPDB_ERR_COND     4014
#define PPDB_ERR_RWLOCK   4015
#define PPDB_ERR_STATE    4016
#define PPDB_ERR_MEMORY_LIMIT 4017
#define PPDB_ERR_CLOSED   4018

// Error type
typedef int ppdb_error_t;

// Error severity levels
typedef enum ppdb_error_severity_e {
    PPDB_ERROR_SEVERITY_INFO = 0,    // 信息性错误
    PPDB_ERROR_SEVERITY_WARNING,     // 警告性错误
    PPDB_ERROR_SEVERITY_ERROR,       // 一般错误
    PPDB_ERROR_SEVERITY_FATAL        // 致命错误
} ppdb_error_severity_t;

// Error category
typedef enum ppdb_error_category_e {
    PPDB_ERROR_CATEGORY_SYSTEM = 0,  // 系统错误
    PPDB_ERROR_CATEGORY_MEMORY,      // 内存错误
    PPDB_ERROR_CATEGORY_IO,          // IO错误
    PPDB_ERROR_CATEGORY_NETWORK,     // 网络错误
    PPDB_ERROR_CATEGORY_PROTOCOL,    // 协议错误
    PPDB_ERROR_CATEGORY_DATA,        // 数据错误
    PPDB_ERROR_CATEGORY_CONFIG,      // 配置错误
    PPDB_ERROR_CATEGORY_USER         // 用户错误
} ppdb_error_category_t;

// Error statistics
typedef struct ppdb_error_stats_s {
    uint64_t total_errors;           // 总错误数
    uint64_t errors_by_severity[4];  // 按严重程度统计
    uint64_t errors_by_category[8];  // 按分类统计
    uint64_t last_error_time;        // 最后一次错误时间
    uint64_t error_free_time;        // 无错误运行时间
} ppdb_error_stats_t;

// Error stack frame
typedef struct ppdb_error_frame_s {
    const char* file;                // 源文件
    const char* func;                // 函数名
    int line;                        // 行号
    char message[256];               // 错误信息
    struct ppdb_error_frame_s* next; // 下一帧
} ppdb_error_frame_t;

// Error callback
typedef void (*ppdb_error_callback_t)(ppdb_error_t code,
                                    ppdb_error_severity_t severity,
                                    ppdb_error_category_t category,
                                    const char* message,
                                    void* user_data);

// Error context structure
typedef struct ppdb_error_context_s {
    ppdb_error_t code;                      // 错误码
    ppdb_error_severity_t severity;         // 严重程度
    ppdb_error_category_t category;         // 错误分类
    const char* file;                       // 源文件
    int line;                              // 行号
    const char* func;                      // 函数名
    char message[PPDB_MAX_ERROR_MESSAGE];  // 错误信息
    ppdb_error_frame_t* stack;             // 错误堆栈
    ppdb_error_stats_t stats;              // 错误统计
    ppdb_error_callback_t callback;        // 错误回调
    void* callback_data;                   // 回调用户数据
} ppdb_error_context_t;

// Error functions
ppdb_error_t ppdb_error_init(void);
void ppdb_error_cleanup(void);

ppdb_error_t ppdb_error_set(ppdb_error_t code,
                          ppdb_error_severity_t severity,
                          ppdb_error_category_t category,
                          const char* file,
                          int line,
                          const char* func,
                          const char* fmt, ...);

ppdb_error_t ppdb_error_push_frame(const char* file,
                                 int line,
                                 const char* func,
                                 const char* fmt, ...);

void ppdb_error_pop_frame(void);

ppdb_error_t ppdb_error_set_callback(ppdb_error_callback_t callback,
                                   void* user_data);

ppdb_error_t ppdb_error_get_stats(ppdb_error_stats_t* stats);

void ppdb_error_clear_stats(void);

ppdb_error_t ppdb_error_set_context(const ppdb_error_context_t* ctx);
const ppdb_error_context_t* ppdb_error_get_context(void);
void ppdb_error_clear_context(void);
ppdb_error_t ppdb_error_get_code(void);
bool ppdb_error_is_error(ppdb_error_t code);
const char* ppdb_error_to_string(ppdb_error_t error);

// Forward declarations
typedef struct ppdb_base_s ppdb_base_t;
typedef struct ppdb_base_mutex_s ppdb_base_mutex_t;
typedef struct ppdb_base_sync_s ppdb_base_sync_t;
typedef struct ppdb_base_thread_s ppdb_base_thread_t;
typedef struct ppdb_base_counter_s ppdb_base_counter_t;
typedef struct ppdb_base_skiplist_s ppdb_base_skiplist_t;
typedef struct ppdb_base_skiplist_node_s ppdb_base_skiplist_node_t;
typedef struct ppdb_base_skiplist_iterator_s ppdb_base_skiplist_iterator_t;
typedef struct ppdb_base_async_loop_s ppdb_base_async_loop_t;
typedef struct ppdb_base_async_handle_s ppdb_base_async_handle_t;
typedef struct ppdb_base_io_manager_s ppdb_base_io_manager_t;
typedef struct ppdb_base_mempool_s ppdb_base_mempool_t;
typedef struct ppdb_base_mempool_block_s ppdb_base_mempool_block_t;
typedef struct ppdb_base_timer_s ppdb_base_timer_t;
typedef struct ppdb_base_timer_stats_s ppdb_base_timer_stats_t;
typedef struct ppdb_base_rwlock_s ppdb_base_rwlock_t;
typedef struct ppdb_base_cond_s ppdb_base_cond_t;
typedef struct ppdb_base_async_task_s ppdb_base_async_task_t;
typedef struct ppdb_base_async_queue_s ppdb_base_async_queue_t;
typedef struct ppdb_base_event_loop_s ppdb_base_event_loop_t;
typedef struct ppdb_connection_s ppdb_connection_t;
typedef struct ppdb_base_io_worker_s ppdb_base_io_worker_t;

// Function types
typedef void (*ppdb_base_thread_func_t)(void* arg);
typedef void (*ppdb_base_async_func_t)(void* arg);
typedef void (*ppdb_base_io_func_t)(void* arg);
typedef int (*ppdb_base_compare_func_t)(const void* a, const void* b);
typedef void (*ppdb_base_cleanup_func_t)(void* data);
typedef void (*ppdb_base_timer_callback_t)(ppdb_base_timer_t* timer, void* user_data);
typedef void (*ppdb_base_async_callback_t)(ppdb_error_t error, void* arg);

// Base configuration
typedef struct ppdb_base_config_s {
    size_t memory_limit;      // Maximum memory usage in bytes
    size_t thread_pool_size;  // Number of threads in the pool
    bool thread_safe;         // Whether to enable thread safety
    bool enable_logging;      // Whether to enable logging
    int log_level;           // Log level
} ppdb_base_config_t;

// Base context
struct ppdb_base_s {
    ppdb_base_config_t config;
    bool initialized;
    ppdb_base_mutex_t* lock;
    ppdb_base_mempool_t* mempool;
    ppdb_base_async_loop_t* async_loop;
    ppdb_base_io_manager_t* io_manager;
};

// Spinlock structure
typedef struct ppdb_base_spinlock_s {
    _Atomic(bool) locked;
    bool initialized;
    bool stats_enabled;
    uint64_t contention_count;
} ppdb_base_spinlock_t;

// Counter structure
typedef struct ppdb_base_counter_s {
    _Atomic(uint64_t) value;
    char* name;
    bool stats_enabled;
} ppdb_base_counter_t;

// Skiplist node structure
typedef struct ppdb_base_skiplist_node_s {
    void* key;
    void* value;
    size_t key_size;
    size_t value_size;
    int level;
    struct ppdb_base_skiplist_node_s** forward;
} ppdb_base_skiplist_node_t;

// Skiplist structure
typedef struct ppdb_base_skiplist_s {
    ppdb_base_skiplist_node_t* head;
    size_t level;
    size_t count;
    ppdb_base_compare_func_t compare;
    ppdb_base_cleanup_func_t cleanup;
    ppdb_base_mutex_t* lock;
} ppdb_base_skiplist_t;

// Skiplist iterator structure
typedef struct ppdb_base_skiplist_iterator_s {
    ppdb_base_skiplist_t* list;
    ppdb_base_skiplist_node_t* current;
    bool reverse;
} ppdb_base_skiplist_iterator_t;

// Memory pool block structure
typedef struct ppdb_base_mempool_block_s {
    void* data;
    size_t used;
    size_t size;
    struct ppdb_base_mempool_block_s* next;
} ppdb_base_mempool_block_t;

// Memory pool structure
typedef struct ppdb_base_mempool_s {
    ppdb_base_mempool_block_t* head;
    size_t block_size;
    size_t alignment;
    
    // 添加统计字段
    size_t total_allocated;
    size_t total_used;
    size_t total_blocks;
    size_t total_allocations;
    size_t total_frees;
    size_t peak_allocated;
    size_t peak_used;
    ppdb_base_mutex_t* lock;      // 用于并发访问保护
} ppdb_base_mempool_t;

// Timer statistics
typedef struct ppdb_base_timer_stats_s {
    uint64_t total_calls;
    uint64_t total_elapsed;
    uint64_t min_elapsed;
    uint64_t max_elapsed;
    uint64_t last_elapsed;
    uint64_t drift;
} ppdb_base_timer_stats_t;

// Timer
typedef struct ppdb_base_timer_s {
    uint64_t interval_ms;
    uint64_t next_timeout;
    ppdb_base_timer_stats_t stats;
    void (*callback)(struct ppdb_base_timer_s* timer, void* user_data);
    void* user_data;
    struct ppdb_base_timer_s* next;
    bool repeating;
} ppdb_base_timer_t;

// Base functions
ppdb_error_t ppdb_base_init(ppdb_base_t** base, const ppdb_base_config_t* config);
void ppdb_base_destroy(ppdb_base_t* base);

// Memory management
ppdb_error_t ppdb_base_mem_malloc(size_t size, void** out_ptr);
void ppdb_base_mem_free(void* ptr);
void* ppdb_base_realloc(void* ptr, size_t size);
ppdb_error_t ppdb_base_mem_calloc(size_t count, size_t size, void** out_ptr);
void* ppdb_base_aligned_alloc(size_t alignment, size_t size);
void ppdb_base_aligned_free(void* ptr);

// Thread functions
ppdb_error_t ppdb_base_thread_create(ppdb_base_thread_t** thread, ppdb_base_thread_func_t func, void* arg);
ppdb_error_t ppdb_base_thread_destroy(ppdb_base_thread_t* thread);
ppdb_error_t ppdb_base_thread_join(ppdb_base_thread_t* thread);
ppdb_error_t ppdb_base_thread_detach(ppdb_base_thread_t* thread);
ppdb_error_t ppdb_base_thread_set_affinity(ppdb_base_thread_t* thread, int cpu_id);
ppdb_error_t ppdb_base_yield(void);
ppdb_error_t ppdb_base_sleep(uint32_t milliseconds);

// Mutex statistics
typedef struct ppdb_base_mutex_stats_s {
    bool enabled;
    uint64_t contention;
} ppdb_base_mutex_stats_t;

// Mutex structure
typedef struct ppdb_base_mutex_s {
    pthread_mutex_t mutex;
    ppdb_base_mutex_stats_t stats;
} ppdb_base_mutex_t;

// Mutex functions
ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex);
ppdb_error_t ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_trylock(ppdb_base_mutex_t* mutex);
void ppdb_base_mutex_enable_stats(ppdb_base_mutex_t* mutex, bool enable);

// Counter functions
ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter, const char* name);
ppdb_error_t ppdb_base_counter_destroy(ppdb_base_counter_t* counter);
ppdb_error_t ppdb_base_counter_increment(ppdb_base_counter_t* counter);
ppdb_error_t ppdb_base_counter_decrement(ppdb_base_counter_t* counter);
ppdb_error_t ppdb_base_counter_get(ppdb_base_counter_t* counter, uint64_t* out_value);
ppdb_error_t ppdb_base_counter_set(ppdb_base_counter_t* counter, uint64_t value);

// Skiplist functions
ppdb_error_t ppdb_base_skiplist_create(ppdb_base_skiplist_t** list, ppdb_base_compare_func_t compare);
ppdb_error_t ppdb_base_skiplist_destroy(ppdb_base_skiplist_t* list);
ppdb_error_t ppdb_base_skiplist_insert(ppdb_base_skiplist_t* list, const void* key, size_t key_size, const void* value, size_t value_size);
ppdb_error_t ppdb_base_skiplist_remove(ppdb_base_skiplist_t* list, const void* key, size_t key_size);
ppdb_error_t ppdb_base_skiplist_find(ppdb_base_skiplist_t* list, const void* key, size_t key_size, void** value, size_t* value_size);

// Skiplist iterator functions
ppdb_error_t ppdb_base_skiplist_iterator_create(ppdb_base_skiplist_t* list, ppdb_base_skiplist_iterator_t** iterator, bool reverse);
ppdb_error_t ppdb_base_skiplist_iterator_destroy(ppdb_base_skiplist_iterator_t* iterator);
ppdb_error_t ppdb_base_skiplist_iterator_valid(ppdb_base_skiplist_iterator_t* iterator, bool* out_valid);
ppdb_error_t ppdb_base_skiplist_iterator_next(ppdb_base_skiplist_iterator_t* iterator);
ppdb_error_t ppdb_base_skiplist_iterator_key(ppdb_base_skiplist_iterator_t* iterator, void** key, size_t* key_size);
ppdb_error_t ppdb_base_skiplist_iterator_value(ppdb_base_skiplist_iterator_t* iterator, void** value, size_t* value_size);

// Memory pool functions
ppdb_error_t ppdb_base_mempool_create(ppdb_base_mempool_t** pool, size_t block_size, size_t alignment);
ppdb_error_t ppdb_base_mempool_destroy(ppdb_base_mempool_t* pool);
void* ppdb_base_mempool_alloc(ppdb_base_mempool_t* pool, size_t size);
void ppdb_base_mempool_free(ppdb_base_mempool_t* pool, void* ptr);

// Timer functions
ppdb_error_t ppdb_base_timer_create(ppdb_base_timer_t** timer, uint64_t interval_ms);
ppdb_error_t ppdb_base_timer_destroy(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_start(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_stop(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_get_stats(ppdb_base_timer_t* timer,
                                     uint64_t* total_ticks,
                                     uint64_t* min_elapsed,
                                     uint64_t* max_elapsed,
                                     uint64_t* avg_elapsed,
                                     uint64_t* last_elapsed,
                                     uint64_t* drift);
void ppdb_base_timer_get_manager_stats(uint64_t* total_timers,
                                    uint64_t* active_timers,
                                    uint64_t* expired_timers,
                                    uint64_t* overdue_timers,
                                    uint64_t* total_drift);
ppdb_error_t ppdb_base_timer_update(void);

// IO manager functions
ppdb_error_t ppdb_base_io_manager_create(ppdb_base_io_manager_t** manager, size_t queue_size, size_t num_threads);
ppdb_error_t ppdb_base_io_manager_destroy(ppdb_base_io_manager_t* manager);
ppdb_error_t ppdb_base_io_manager_process(ppdb_base_io_manager_t* manager);

// Async functions
ppdb_error_t ppdb_base_async_schedule(ppdb_base_t* base, ppdb_base_async_func_t fn, void* arg, ppdb_base_async_handle_t** handle);
ppdb_error_t ppdb_base_async_cancel(ppdb_base_async_handle_t* handle);

// Thread structure
typedef struct ppdb_base_thread_s {
    pthread_t thread;
} ppdb_base_thread_t;

// Condition variable structure
typedef struct ppdb_base_cond_s {
    pthread_cond_t cond;
} ppdb_base_cond_t;

// Read-write lock structure
typedef struct ppdb_base_rwlock_s {
    pthread_rwlock_t rwlock;
} ppdb_base_rwlock_t;

// Async task structure
typedef struct ppdb_base_async_task_s {
    ppdb_base_async_func_t func;
    void* arg;
    uint32_t priority;
    uint64_t timeout_us;
    uint64_t start_time;
    _Atomic(uint32_t) state;
    ppdb_base_async_callback_t callback;
    void* callback_arg;
    struct ppdb_base_async_task_s* next;
} ppdb_base_async_task_t;

// Async queue structure
typedef struct ppdb_base_async_queue_s {
    ppdb_base_async_task_t* head;
    ppdb_base_async_task_t* tail;
    size_t size;
} ppdb_base_async_queue_t;

// Async loop structure
typedef struct ppdb_base_async_loop_s {
    ppdb_base_mutex_t* lock;
    ppdb_base_cond_t* cond;
    ppdb_base_async_queue_t queues[3];  // Priority queues
    ppdb_base_async_io_stats_t io_stats; // IO 统计信息
    bool running;
} ppdb_base_async_loop_t;

// Async handle structure
typedef struct ppdb_base_async_handle_s {
    ppdb_base_async_task_t* task;
    ppdb_base_async_loop_t* loop;
} ppdb_base_async_handle_t;

// IO request structure
typedef struct ppdb_base_io_request_s {
    void (*func)(void* arg);
    void* arg;
    int priority;
    struct ppdb_base_io_request_s* next;
} ppdb_base_io_request_t;

// IO queue structure
typedef struct ppdb_base_io_queue_s {
    ppdb_base_io_request_t* head;
    ppdb_base_io_request_t* tail;
    size_t size;
} ppdb_base_io_queue_t;

// IO manager structure
typedef struct ppdb_base_io_manager_s {
    ppdb_base_mutex_t* mutex;
    ppdb_base_cond_t* cond;
    ppdb_base_io_worker_t** workers;
    ppdb_base_io_queue_t queues[PPDB_IO_QUEUE_PRIORITIES];
    size_t max_queue_size;
    size_t min_threads;
    size_t active_threads;
    bool running;
} ppdb_base_io_manager_t;

// Event handler structure
typedef struct ppdb_base_event_handler_s {
    int fd;
    uint32_t events;
    void (*callback)(struct ppdb_base_event_handler_s* handler, uint32_t events);
    void* user_data;
    struct ppdb_base_event_handler_s* next;
} ppdb_base_event_handler_t;

// Event filter
typedef struct ppdb_base_event_filter_s {
    bool (*filter)(void* handler, uint32_t events);
    void* user_data;
} ppdb_base_event_filter_t;

// Event implementation operations
typedef struct ppdb_base_event_impl_ops_s {
    ppdb_error_t (*init)(void** context);
    void (*cleanup)(void* context);
    ppdb_error_t (*add)(void* context, ppdb_base_event_handler_t* handler);
    ppdb_error_t (*remove)(void* context, ppdb_base_event_handler_t* handler);
    ppdb_error_t (*modify)(void* context, ppdb_base_event_handler_t* handler);
    ppdb_error_t (*wait)(void* context, int timeout_ms);
} ppdb_base_event_impl_ops_t;

// Utility functions
uint64_t ppdb_base_get_time_ns(void);
ppdb_error_t ppdb_base_time_get_microseconds(uint64_t* out_time);
ppdb_error_t ppdb_base_sleep_us(uint32_t microseconds);

// RWLock functions
ppdb_error_t ppdb_base_rwlock_create(ppdb_base_rwlock_t** rwlock);
ppdb_error_t ppdb_base_rwlock_destroy(ppdb_base_rwlock_t* rwlock);
ppdb_error_t ppdb_base_rwlock_rdlock(ppdb_base_rwlock_t* rwlock);
ppdb_error_t ppdb_base_rwlock_wrlock(ppdb_base_rwlock_t* rwlock);
ppdb_error_t ppdb_base_rwlock_unlock(ppdb_base_rwlock_t* rwlock);

// Memory statistics structure
typedef struct ppdb_base_memory_stats_s {
    size_t total_allocated;
    size_t total_freed;
    size_t current_usage;
    size_t peak_usage;
    size_t memory_limit;
} ppdb_base_memory_stats_t;

// Memory pool statistics structure
typedef struct ppdb_base_mempool_stats_s {
    size_t total_allocated;         // 总分配字节数
    size_t total_used;             // 实际使用字节数
    size_t total_blocks;           // 总块数
    size_t total_allocations;      // 总分配次数
    size_t total_frees;            // 总释放次数
    size_t peak_allocated;         // 峰值分配字节数
    size_t peak_used;             // 峰值使用字节数
    size_t block_size;            // 块大小
    size_t alignment;             // 对齐大小
    size_t fragmentation;         // 内存碎片(字节)
} ppdb_base_mempool_stats_t;

// Memory management functions
void ppdb_base_set_memory_limit(size_t limit);
void ppdb_base_get_memory_stats(ppdb_base_memory_stats_t* stats);
void ppdb_base_mempool_get_stats(ppdb_base_mempool_t* pool, ppdb_base_mempool_stats_t* stats);

// Condition variable functions
ppdb_error_t ppdb_base_cond_create(ppdb_base_cond_t** cond);
ppdb_error_t ppdb_base_cond_destroy(ppdb_base_cond_t* cond);
ppdb_error_t ppdb_base_cond_wait(ppdb_base_cond_t* cond, ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_cond_timedwait(ppdb_base_cond_t* cond, ppdb_base_mutex_t* mutex, uint64_t timeout_us);
ppdb_error_t ppdb_base_cond_signal(ppdb_base_cond_t* cond);
ppdb_error_t ppdb_base_cond_broadcast(ppdb_base_cond_t* cond);

// Async callback type
typedef void (*ppdb_base_async_callback_t)(ppdb_error_t error, void* arg);

// Async functions
ppdb_error_t ppdb_base_async_loop_create(ppdb_base_async_loop_t** loop);
ppdb_error_t ppdb_base_async_loop_destroy(ppdb_base_async_loop_t* loop);
ppdb_error_t ppdb_base_async_submit(ppdb_base_async_loop_t* loop,
                                   ppdb_base_async_func_t func,
                                   void* arg,
                                   uint32_t priority,
                                   uint64_t timeout_us,
                                   ppdb_base_async_callback_t callback,
                                   void* callback_arg,
                                   ppdb_base_async_handle_t** handle);
ppdb_error_t ppdb_base_async_cancel(ppdb_base_async_handle_t* handle);
void ppdb_base_async_wait_all(ppdb_base_async_loop_t* loop);

// List types
typedef struct ppdb_base_list_node_s {
    void* data;
    struct ppdb_base_list_node_s* next;
    struct ppdb_base_list_node_s* prev;
} ppdb_base_list_node_t;

typedef struct ppdb_base_list_s {
    ppdb_base_list_node_t* head;
    ppdb_base_list_node_t* tail;
    size_t size;
    ppdb_base_mutex_t* lock;
    void (*cleanup)(void*);
} ppdb_base_list_t;

// Hash types
typedef struct ppdb_base_hash_node_s {
    void* key;
    void* value;
    struct ppdb_base_hash_node_s* next;
} ppdb_base_hash_node_t;

typedef struct ppdb_base_hash_s {
    ppdb_base_hash_node_t** buckets;
    size_t size;
    size_t capacity;
    ppdb_base_compare_func_t compare;
    ppdb_base_cleanup_func_t cleanup;
    ppdb_base_mutex_t* lock;
} ppdb_base_hash_t;

// Cleanup function type
typedef void (*ppdb_base_cleanup_func_t)(void*);

// Network types
typedef struct ppdb_net_config_s {
    const char* host;
    uint16_t port;
    size_t max_connections;
    size_t io_threads;
    size_t read_buffer_size;
    size_t write_buffer_size;
    int backlog;
} ppdb_net_config_t;

// Protocol operations
typedef struct ppdb_protocol_ops_s {
    ppdb_error_t (*create)(void** proto, void* proto_data);
    void (*destroy)(void* proto);
    ppdb_error_t (*on_data)(void* proto, struct ppdb_connection_s* conn, const void* data, size_t size);
    ppdb_error_t (*on_close)(void* proto, struct ppdb_connection_s* conn);
} ppdb_protocol_ops_t;

// Connection states
typedef enum ppdb_connection_state_e {
    PPDB_CONN_STATE_INIT = 0,    // Initial state
    PPDB_CONN_STATE_ACTIVE,      // Active state
    PPDB_CONN_STATE_IDLE,        // Idle state
    PPDB_CONN_STATE_CLOSING,     // Closing state
    PPDB_CONN_STATE_CLOSED       // Closed state
} ppdb_connection_state_t;

// Connection structure
typedef struct ppdb_connection_s {
    int fd;                           // File descriptor
    struct ppdb_net_server_s* server; // Server instance
    void* recv_buffer;                // Receive buffer
    size_t recv_size;                // Received data size
    size_t buffer_size;              // Buffer capacity
    
    // Connection state
    ppdb_connection_state_t state;    // Current state
    uint64_t last_active_time;        // Last activity timestamp
    uint32_t idle_timeout;            // Idle timeout in ms
    uint32_t connect_time;            // Connection establishment time
    
    // Statistics
    uint64_t bytes_received;          // Total bytes received
    uint64_t bytes_sent;              // Total bytes sent
    uint32_t request_count;           // Request counter
    uint32_t error_count;             // Error counter
} ppdb_connection_t;

// Server structure
typedef struct ppdb_net_server_s {
    int listen_fd;
    bool running;
    ppdb_base_thread_t** io_threads;
    size_t thread_count;
    ppdb_base_event_loop_t* event_loop;
    void* user_data;
} ppdb_net_server_t;

// String Operations
ppdb_error_t ppdb_base_string_equal(const char* s1, const char* s2, bool* out_result);
ppdb_error_t ppdb_base_string_hash(const char* str, size_t* out_hash);

// File System Operations
ppdb_error_t ppdb_base_fs_exists(const char* path, bool* out_exists);
ppdb_error_t ppdb_base_fs_create_directory(const char* path);

// Time and System Functions
ppdb_error_t ppdb_base_time_get_microseconds(uint64_t* out_time);
ppdb_error_t ppdb_base_sys_get_cpu_count(uint32_t* out_count);
ppdb_error_t ppdb_base_sys_get_page_size(size_t* out_size);

// List Operations
ppdb_error_t ppdb_base_list_init(ppdb_base_list_t* list);
ppdb_error_t ppdb_base_list_destroy(ppdb_base_list_t* list);
ppdb_error_t ppdb_base_list_push_front(ppdb_base_list_t* list, void* data);
ppdb_error_t ppdb_base_list_push_back(ppdb_base_list_t* list, void* data);
ppdb_error_t ppdb_base_list_pop_front(ppdb_base_list_t* list, void** out_data);
ppdb_error_t ppdb_base_list_pop_back(ppdb_base_list_t* list, void** out_data);
ppdb_error_t ppdb_base_list_front(ppdb_base_list_t* list, void** out_data);
ppdb_error_t ppdb_base_list_back(ppdb_base_list_t* list, void** out_data);
ppdb_error_t ppdb_base_list_size(ppdb_base_list_t* list, size_t* out_size);
ppdb_error_t ppdb_base_list_empty(ppdb_base_list_t* list, bool* out_empty);
ppdb_error_t ppdb_base_list_clear(ppdb_base_list_t* list);
ppdb_error_t ppdb_base_list_reverse(ppdb_base_list_t* list);

// Hash Table Operations
ppdb_error_t ppdb_base_hash_init(ppdb_base_hash_t* hash, size_t initial_size);
ppdb_error_t ppdb_base_hash_destroy(ppdb_base_hash_t* hash);

// Skip List Operations
ppdb_error_t ppdb_base_skiplist_init(ppdb_base_skiplist_t* list, size_t max_level);
ppdb_error_t ppdb_base_skiplist_destroy(ppdb_base_skiplist_t* list);
ppdb_error_t ppdb_base_skiplist_size(ppdb_base_skiplist_t* list, size_t* out_size);
ppdb_error_t ppdb_base_skiplist_iterator_valid(ppdb_base_skiplist_iterator_t* iterator, bool* out_valid);

// Counter Operations
ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter, const char* name);
ppdb_error_t ppdb_base_counter_destroy(ppdb_base_counter_t* counter);
ppdb_error_t ppdb_base_counter_get(ppdb_base_counter_t* counter, uint64_t* out_value);

// Async Operations
ppdb_error_t ppdb_base_async_loop_create(ppdb_base_async_loop_t** loop);
ppdb_error_t ppdb_base_async_loop_destroy(ppdb_base_async_loop_t* loop);
ppdb_error_t ppdb_base_async_loop_run(ppdb_base_async_loop_t* loop, int timeout_ms);

// Timer Operations
ppdb_error_t ppdb_base_timer_create(ppdb_base_timer_t** timer, uint64_t interval_ms);
ppdb_error_t ppdb_base_timer_destroy(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_update(void);

// Network Operations
ppdb_error_t ppdb_base_net_server_create(ppdb_net_server_t** server);
ppdb_error_t ppdb_base_net_server_start(ppdb_net_server_t* server);
ppdb_error_t ppdb_base_net_server_stop(ppdb_net_server_t* server);
ppdb_error_t ppdb_base_net_server_destroy(ppdb_net_server_t* server);

// Configuration functions
ppdb_error_t ppdb_base_config_load(const char* config_path);
ppdb_error_t ppdb_base_config_set(const char* key, const char* value);

// IO worker structure
typedef struct ppdb_base_io_worker_s {
    ppdb_base_thread_t* thread;
    ppdb_base_io_manager_t* mgr;
    int cpu_id;
    bool running;
} ppdb_base_io_worker_t;

// Event loop functions
ppdb_error_t ppdb_base_event_loop_create(ppdb_base_event_loop_t** loop);
ppdb_error_t ppdb_base_event_loop_destroy(ppdb_base_event_loop_t* loop);
ppdb_error_t ppdb_base_event_handler_add(ppdb_base_event_loop_t* loop, ppdb_base_event_handler_t* handler);
ppdb_error_t ppdb_base_event_handler_remove(ppdb_base_event_loop_t* loop, ppdb_base_event_handler_t* handler);
ppdb_error_t ppdb_base_event_loop_run(ppdb_base_event_loop_t* loop, int timeout_ms);

// Memory management functions
ppdb_error_t ppdb_base_mem_malloc(size_t size, void** out_ptr);
ppdb_error_t ppdb_base_mem_calloc(size_t count, size_t size, void** out_ptr);
ppdb_error_t ppdb_base_mem_realloc(void* ptr, size_t new_size, void** out_ptr);
void ppdb_base_mem_free(void* ptr);

// Network server functions
ppdb_error_t ppdb_base_net_server_stop(ppdb_net_server_t* server);
ppdb_error_t ppdb_base_net_server_destroy(ppdb_net_server_t* server);

// Event loop structure
typedef struct ppdb_base_event_loop_s {
    bool running;
    ppdb_base_event_handler_t* handlers;
    size_t handler_count;
    ppdb_base_mutex_t* lock;
    int epoll_fd;  // For Linux
    void* kqueue_fd;  // For BSD/macOS
    void* iocp_handle;  // For Windows
} ppdb_base_event_loop_t;

// Event loop functions
ppdb_error_t ppdb_base_event_loop_create(ppdb_base_event_loop_t** loop);
ppdb_error_t ppdb_base_event_loop_destroy(ppdb_base_event_loop_t* loop);
ppdb_error_t ppdb_base_event_handler_add(ppdb_base_event_loop_t* loop, ppdb_base_event_handler_t* handler);
ppdb_error_t ppdb_base_event_handler_remove(ppdb_base_event_loop_t* loop, ppdb_base_event_handler_t* handler);
ppdb_error_t ppdb_base_event_loop_run(ppdb_base_event_loop_t* loop, int timeout_ms);

// 异步IO操作类型
typedef enum ppdb_base_aio_op_e {
    PPDB_AIO_OP_READ = 0,
    PPDB_AIO_OP_WRITE,
    PPDB_AIO_OP_FSYNC,
    PPDB_AIO_OP_FDSYNC
} ppdb_base_aio_op_t;

// 异步IO请求状态
typedef enum ppdb_base_aio_state_e {
    PPDB_AIO_STATE_INIT = 0,    // 初始状态
    PPDB_AIO_STATE_PENDING,     // 等待执行
    PPDB_AIO_STATE_RUNNING,     // 正在执行
    PPDB_AIO_STATE_COMPLETED,   // 已完成
    PPDB_AIO_STATE_ERROR       // 执行出错
} ppdb_base_aio_state_t;

// 异步IO统计信息
typedef struct ppdb_base_aio_stats_s {
    uint64_t total_requests;     // 总请求数
    uint64_t completed_requests; // 已完成请求数
    uint64_t failed_requests;    // 失败请求数
    uint64_t bytes_read;        // 总读取字节数
    uint64_t bytes_written;     // 总写入字节数
    uint64_t total_wait_time;   // 总等待时间(微秒)
    uint64_t total_exec_time;   // 总执行时间(微秒)
} ppdb_base_aio_stats_t;

// 异步IO回调函数
typedef void (*ppdb_base_aio_callback_t)(void* request, void* user_data);

// 异步IO请求结构
typedef struct ppdb_base_aio_request_s {
    int fd;                     // 文件描述符
    ppdb_base_aio_op_t op;     // 操作类型
    void* buffer;              // 数据缓冲区
    size_t size;               // 缓冲区大小
    off_t offset;              // 文件偏移
    ppdb_base_aio_state_t state; // 请求状态
    ppdb_error_t error;        // 错误码
    ppdb_base_aio_callback_t callback; // 完成回调
    void* user_data;           // 用户数据
    uint64_t submit_time;      // 提交时间
    uint64_t start_time;       // 开始时间
    uint64_t complete_time;    // 完成时间
    struct ppdb_base_aio_request_s* next; // 链表指针
} ppdb_base_aio_request_t;

// 异步IO上下文结构
typedef struct ppdb_base_aio_context_s {
    ppdb_base_mutex_t* lock;   // 互斥锁
    ppdb_base_cond_t* cond;    // 条件变量
    ppdb_base_thread_t** workers; // 工作线程
    size_t num_workers;        // 工作线程数
    ppdb_base_aio_request_t* pending_queue; // 等待队列
    ppdb_base_aio_request_t* running_queue; // 运行队列
    ppdb_base_aio_stats_t stats; // 统计信息
    bool running;              // 运行状态
} ppdb_base_aio_context_t;

// 异步IO函数
ppdb_error_t ppdb_base_aio_init(ppdb_base_aio_context_t** ctx, size_t num_workers);
void ppdb_base_aio_cleanup(ppdb_base_aio_context_t* ctx);

ppdb_error_t ppdb_base_aio_submit(ppdb_base_aio_context_t* ctx,
                                ppdb_base_aio_request_t* request);

ppdb_error_t ppdb_base_aio_wait(ppdb_base_aio_context_t* ctx,
                              ppdb_base_aio_request_t* request,
                              uint32_t timeout_ms);

void ppdb_base_aio_get_stats(ppdb_base_aio_context_t* ctx,
                           ppdb_base_aio_stats_t* stats);

// Async IO functions
ppdb_error_t ppdb_base_async_read(ppdb_base_async_loop_t* loop,
                                int fd,
                                void* buffer,
                                size_t size,
                                off_t offset,
                                ppdb_base_async_callback_t callback,
                                void* user_data);

ppdb_error_t ppdb_base_async_write(ppdb_base_async_loop_t* loop,
                                 int fd,
                                 const void* buffer,
                                 size_t size,
                                 off_t offset,
                                 ppdb_base_async_callback_t callback,
                                 void* user_data);

ppdb_error_t ppdb_base_async_fsync(ppdb_base_async_loop_t* loop,
                                 int fd,
                                 ppdb_base_async_callback_t callback,
                                 void* user_data);

void ppdb_base_async_get_io_stats(ppdb_base_async_loop_t* loop,
                                ppdb_base_async_io_stats_t* stats);

#endif // PPDB_BASE_H