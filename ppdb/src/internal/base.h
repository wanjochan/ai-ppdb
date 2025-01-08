/*
 * base.h - Base Layer Definitions
 */

#ifndef PPDB_BASE_H
#define PPDB_BASE_H

#include <cosmopolitan.h>

// Constants
#define PPDB_MAX_ERROR_MESSAGE 256
#define PPDB_MAX_SKIPLIST_LEVEL 32

// Common status codes
#define PPDB_OK 0

// Error codes (4000-4199)
#define PPDB_BASE_ERR_START    4000
#define PPDB_BASE_ERR_PARAM    4001
#define PPDB_BASE_ERR_MEMORY   4002
#define PPDB_BASE_ERR_SYSTEM   4003
#define PPDB_BASE_ERR_NOT_FOUND 4004
#define PPDB_BASE_ERR_EXISTS   4005
#define PPDB_BASE_ERR_TIMEOUT  4006
#define PPDB_BASE_ERR_BUSY     4007
#define PPDB_BASE_ERR_FULL     4008
#define PPDB_BASE_ERR_EMPTY    4009
#define PPDB_BASE_ERR_IO       4010
#define PPDB_BASE_ERR_INTERNAL 4011

// Error type
typedef int ppdb_error_t;

// Error context structure
typedef struct ppdb_error_context_s {
    ppdb_error_t code;
    const char* file;
    int line;
    const char* func;
    char message[PPDB_MAX_ERROR_MESSAGE];
} ppdb_error_context_t;

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

// Function types
typedef void (*ppdb_base_thread_func_t)(void* arg);
typedef void (*ppdb_base_async_func_t)(void* arg);
typedef void (*ppdb_base_io_func_t)(void* arg);
typedef int (*ppdb_base_compare_func_t)(const void* a, const void* b);
typedef void (*ppdb_base_timer_callback_t)(ppdb_base_timer_t* timer, void* user_data);

// Base configuration
typedef struct ppdb_base_config_s {
    size_t memory_limit;      // Maximum memory usage in bytes
    size_t thread_pool_size;  // Number of threads in the pool
    bool thread_safe;         // Whether to enable thread safety
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
    int max_level;
    size_t size;
    ppdb_base_compare_func_t compare;
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
} ppdb_base_mempool_t;

// Timer statistics structure
typedef struct ppdb_base_timer_stats_s {
    uint64_t total_ticks;
    uint64_t total_elapsed;
    uint64_t min_elapsed;
    uint64_t max_elapsed;
    uint64_t avg_elapsed;
} ppdb_base_timer_stats_t;

// Timer structure
typedef struct ppdb_base_timer_s {
    ppdb_base_timer_callback_t callback;
    void* user_data;
    uint64_t interval_ms;
    uint64_t timeout_us;
    uint64_t next_timeout;
    bool repeat;
    bool active;
    ppdb_base_timer_stats_t stats;
} ppdb_base_timer_t;

// Base functions
ppdb_error_t ppdb_base_init(ppdb_base_t** base, const ppdb_base_config_t* config);
void ppdb_base_destroy(ppdb_base_t* base);

// Memory management
void* ppdb_base_malloc(size_t size);
void ppdb_base_free(void* ptr);
void* ppdb_base_realloc(void* ptr, size_t size);
void* ppdb_base_calloc(size_t count, size_t size);
void* ppdb_base_aligned_alloc(size_t alignment, size_t size);
void ppdb_base_aligned_free(void* ptr);

// Thread functions
ppdb_error_t ppdb_base_thread_create(ppdb_base_thread_t** thread, ppdb_base_thread_func_t func, void* arg);
ppdb_error_t ppdb_base_thread_destroy(ppdb_base_thread_t* thread);
ppdb_error_t ppdb_base_thread_join(ppdb_base_thread_t* thread);
ppdb_error_t ppdb_base_thread_detach(ppdb_base_thread_t* thread);
ppdb_error_t ppdb_base_yield(void);
ppdb_error_t ppdb_base_sleep(uint32_t milliseconds);

// Mutex functions
ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex);
ppdb_error_t ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_trylock(ppdb_base_mutex_t* mutex);

// Counter functions
ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter, const char* name);
ppdb_error_t ppdb_base_counter_destroy(ppdb_base_counter_t* counter);
ppdb_error_t ppdb_base_counter_increment(ppdb_base_counter_t* counter);
ppdb_error_t ppdb_base_counter_decrement(ppdb_base_counter_t* counter);
uint64_t ppdb_base_counter_get(ppdb_base_counter_t* counter);
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
bool ppdb_base_skiplist_iterator_valid(ppdb_base_skiplist_iterator_t* iterator);
ppdb_error_t ppdb_base_skiplist_iterator_next(ppdb_base_skiplist_iterator_t* iterator);
ppdb_error_t ppdb_base_skiplist_iterator_key(ppdb_base_skiplist_iterator_t* iterator, void** key, size_t* key_size);
ppdb_error_t ppdb_base_skiplist_iterator_value(ppdb_base_skiplist_iterator_t* iterator, void** value, size_t* value_size);

// Memory pool functions
ppdb_error_t ppdb_base_mempool_create(ppdb_base_mempool_t** pool, size_t block_size, size_t alignment);
ppdb_error_t ppdb_base_mempool_destroy(ppdb_base_mempool_t* pool);
void* ppdb_base_mempool_alloc(ppdb_base_mempool_t* pool, size_t size);
void ppdb_base_mempool_free(ppdb_base_mempool_t* pool, void* ptr);

// Timer functions
ppdb_error_t ppdb_base_timer_create(ppdb_base_timer_t** timer, uint64_t interval_ms, bool repeat, ppdb_base_timer_callback_t callback, void* user_data);
ppdb_error_t ppdb_base_timer_destroy(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_start(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_stop(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_get_stats(ppdb_base_timer_t* timer, ppdb_base_timer_stats_t* stats);

// IO manager functions
ppdb_error_t ppdb_base_io_manager_create(ppdb_base_io_manager_t** manager);
ppdb_error_t ppdb_base_io_manager_destroy(ppdb_base_io_manager_t* manager);
ppdb_error_t ppdb_base_io_manager_process(ppdb_base_io_manager_t* manager);

// Async functions
ppdb_error_t ppdb_base_async_schedule(ppdb_base_t* base, ppdb_base_async_func_t fn, void* arg, ppdb_base_async_handle_t** handle);
ppdb_error_t ppdb_base_async_cancel(ppdb_base_async_handle_t* handle);

// Thread structure
typedef struct ppdb_base_thread_s {
    pthread_t thread;
    bool initialized;
} ppdb_base_thread_t;

// Mutex structure
typedef struct ppdb_base_mutex_s {
    pthread_mutex_t mutex;
    bool initialized;
} ppdb_base_mutex_t;

// Async task structure
typedef struct ppdb_base_async_task_s {
    ppdb_base_async_func_t func;
    void* arg;
    struct ppdb_base_async_task_s* next;
} ppdb_base_async_task_t;

// Async loop structure
typedef struct ppdb_base_async_loop_s {
    ppdb_base_thread_t* worker;
    ppdb_base_mutex_t* mutex;
    ppdb_base_async_task_t* tasks;
    bool running;
} ppdb_base_async_loop_t;

// Async handle structure
typedef struct ppdb_base_async_handle_s {
    ppdb_base_async_func_t fn;
    void* arg;
    struct ppdb_base_async_handle_s* next;
    bool cancelled;
} ppdb_base_async_handle_t;

// IO request structure
typedef struct ppdb_base_io_request_s {
    ppdb_base_io_func_t func;
    void* arg;
    struct ppdb_base_io_request_s* next;
} ppdb_base_io_request_t;

// IO manager structure
typedef struct ppdb_base_io_manager_s {
    ppdb_base_thread_t* worker;
    ppdb_base_mutex_t* mutex;
    ppdb_base_io_request_t* requests;
    bool running;
} ppdb_base_io_manager_t;

// Utility functions
uint64_t ppdb_base_get_time_us(void);

#endif // PPDB_BASE_H