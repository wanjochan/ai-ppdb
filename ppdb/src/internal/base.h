/*
 * base.h - Base Layer Definitions
 */

#ifndef PPDB_BASE_H
#define PPDB_BASE_H

#include <cosmopolitan.h>

// Constants
#define PPDB_MAX_ERROR_MESSAGE 256
#define PPDB_MAX_SKIPLIST_LEVEL 32

// Error codes base
#define PPDB_ERROR_START 0x1000

// Base configuration
typedef struct ppdb_base_config_s {
    size_t memory_limit;
    size_t thread_pool_size;
    bool thread_safe;
} ppdb_base_config_t;

// Base context
typedef struct ppdb_base_s {
    ppdb_base_config_t config;
    bool initialized;
} ppdb_base_t;

// Spinlock structure
typedef struct ppdb_base_spinlock_s {
    _Atomic(bool) locked;
    bool initialized;
    bool stats_enabled;
    uint64_t contention_count;
} ppdb_base_spinlock_t;

// Error codes
#define PPDB_OK 0
#define PPDB_BASE_ERR_PARAM 1
#define PPDB_BASE_ERR_MEMORY 2
#define PPDB_BASE_ERR_SYSTEM 3
#define PPDB_BASE_ERR_NOT_FOUND 4
#define PPDB_BASE_ERR_EXISTS 5
#define PPDB_BASE_ERR_TIMEOUT 6
#define PPDB_BASE_ERR_BUSY 7
#define PPDB_BASE_ERR_FULL 8
#define PPDB_BASE_ERR_EMPTY 9
#define PPDB_BASE_ERR_IO 10

// Error type
typedef int ppdb_error_t;

// Forward declarations
typedef struct ppdb_base_mutex_s ppdb_base_mutex_t;
typedef struct ppdb_base_sync_s ppdb_base_sync_t;
typedef struct ppdb_base_thread_s ppdb_base_thread_t;
typedef struct ppdb_base_counter_s ppdb_base_counter_t;
typedef struct ppdb_base_skiplist_s ppdb_base_skiplist_t;
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

// Error handling
typedef struct ppdb_error_context_s {
    ppdb_error_t code;
    const char* file;
    int line;
    const char* func;
    char message[PPDB_MAX_ERROR_MESSAGE];
} ppdb_error_context_t;

// Mutex structure
struct ppdb_base_mutex_s {
    pthread_mutex_t mutex;
    bool initialized;
};

// Thread structure
struct ppdb_base_thread_s {
    pthread_t thread;
    bool initialized;
};

// Counter structure
struct ppdb_base_counter_s {
    int64_t value;
    ppdb_base_mutex_t* mutex;
};

// Skiplist node structure
struct ppdb_base_skiplist_node_s {
    const void* key;
    void* value;
    struct ppdb_base_skiplist_node_s** forward;
    int level;
};

// Skiplist structure
struct ppdb_base_skiplist_s {
    struct ppdb_base_skiplist_node_s* header;
    int level;
    size_t size;
    ppdb_base_compare_func_t compare;
};

// Async task structure
struct ppdb_base_async_task_s {
    ppdb_base_async_func_t func;
    void* arg;
    struct ppdb_base_async_task_s* next;
};

// Async loop structure
struct ppdb_base_async_loop_s {
    ppdb_base_mutex_t* mutex;
    ppdb_base_thread_t* worker;
    struct ppdb_base_async_task_s* tasks;
    bool running;
};

// IO request structure
struct ppdb_base_io_request_s {
    ppdb_base_io_func_t func;
    void* arg;
    struct ppdb_base_io_request_s* next;
};

// IO manager structure
struct ppdb_base_io_manager_s {
    ppdb_base_mutex_t* mutex;
    ppdb_base_thread_t* worker;
    struct ppdb_base_io_request_s* requests;
    bool running;
};

// Memory pool block structure
struct ppdb_base_mempool_block_s {
    struct ppdb_base_mempool_block_s* next;
    size_t size;
    size_t used;
    void* data;
};

// Memory pool structure
struct ppdb_base_mempool_s {
    ppdb_base_mempool_block_t* head;
    size_t block_size;
    size_t alignment;
};

// Timer statistics structure
struct ppdb_base_timer_stats_s {
    uint64_t total_timeouts;
    uint64_t active_timers;
    uint64_t total_resets;
    uint64_t total_cancels;
    uint64_t peak_timers;
};

// Timer structure
struct ppdb_base_timer_s {
    uint64_t timeout_us;
    uint64_t next_timeout;
    bool repeat;
    void* user_data;
    ppdb_base_timer_callback_t callback;
    ppdb_base_timer_stats_t stats;
};

// Sync configuration structure
typedef struct ppdb_base_sync_config_s {
    bool thread_safe;
    uint32_t spin_count;
    uint32_t backoff_us;
} ppdb_base_sync_config_t;

// Base layer initialization and cleanup
ppdb_error_t ppdb_base_init(ppdb_base_t** base, const ppdb_base_config_t* config);
void ppdb_base_destroy(ppdb_base_t* base);

// Error handling functions
ppdb_error_t ppdb_error_init(void);
void ppdb_error_set_context(ppdb_error_context_t* ctx);
const ppdb_error_context_t* ppdb_error_get_context(void);
void ppdb_error_clear_context(void);
ppdb_error_t ppdb_error_set(ppdb_error_t code, const char* file, int line, const char* func, const char* fmt, ...);
ppdb_error_t ppdb_error_get_code(void);
const char* ppdb_error_get_message(void);
const char* ppdb_error_get_file(void);
int ppdb_error_get_line(void);
const char* ppdb_error_get_func(void);
void ppdb_error_format_message(char* buffer, size_t size);
void ppdb_error_cleanup(void);
const char* ppdb_error_to_string(ppdb_error_t code);

// Mutex functions
ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex);
ppdb_error_t ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_trylock(ppdb_base_mutex_t* mutex);
void ppdb_base_mutex_enable_stats(ppdb_base_mutex_t* mutex, bool enable);

// Thread functions
ppdb_error_t ppdb_base_thread_create(ppdb_base_thread_t** thread, ppdb_base_thread_func_t func, void* arg);
ppdb_error_t ppdb_base_thread_join(ppdb_base_thread_t* thread);
ppdb_error_t ppdb_base_thread_detach(ppdb_base_thread_t* thread);
void ppdb_base_thread_destroy(ppdb_base_thread_t* thread);
void ppdb_base_yield(void);
void ppdb_base_sleep(uint32_t milliseconds);

// Counter functions
ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter);
ppdb_error_t ppdb_base_counter_destroy(ppdb_base_counter_t* counter);
int64_t ppdb_base_counter_get(ppdb_base_counter_t* counter);
void ppdb_base_counter_set(ppdb_base_counter_t* counter, int64_t value);
void ppdb_base_counter_inc(ppdb_base_counter_t* counter);
void ppdb_base_counter_dec(ppdb_base_counter_t* counter);
void ppdb_base_counter_add(ppdb_base_counter_t* counter, int64_t value);
void ppdb_base_counter_sub(ppdb_base_counter_t* counter, int64_t value);
bool ppdb_base_counter_compare_exchange(ppdb_base_counter_t* counter, int64_t expected, int64_t desired);
void ppdb_base_counter_reset(ppdb_base_counter_t* counter);

// Skiplist functions
ppdb_error_t ppdb_base_skiplist_create(ppdb_base_skiplist_t** list, ppdb_base_compare_func_t compare);
ppdb_error_t ppdb_base_skiplist_destroy(ppdb_base_skiplist_t* list);
ppdb_error_t ppdb_base_skiplist_insert(ppdb_base_skiplist_t* list, const void* key, void* value);
ppdb_error_t ppdb_base_skiplist_find(ppdb_base_skiplist_t* list, const void* key, void** value);
ppdb_error_t ppdb_base_skiplist_remove(ppdb_base_skiplist_t* list, const void* key);
size_t ppdb_base_skiplist_size(const ppdb_base_skiplist_t* list);
void ppdb_base_skiplist_clear(ppdb_base_skiplist_t* list);

// Memory pool functions
ppdb_error_t ppdb_base_memory_init(void);
void ppdb_base_memory_cleanup(void);
void* ppdb_base_aligned_alloc(size_t alignment, size_t size);
void ppdb_base_aligned_free(void* ptr);
ppdb_error_t ppdb_base_mempool_create(ppdb_base_mempool_t** pool, size_t block_size, size_t alignment);
void ppdb_base_mempool_destroy(ppdb_base_mempool_t* pool);
void* ppdb_base_mempool_alloc(ppdb_base_mempool_t* pool, size_t size);
void ppdb_base_mempool_free(ppdb_base_mempool_t* pool, void* ptr);

// Timer functions
ppdb_error_t ppdb_base_timer_create(ppdb_base_timer_t** timer);
void ppdb_base_timer_destroy(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_start(ppdb_base_timer_t* timer, uint64_t timeout_ms, bool repeat,
                                  ppdb_base_timer_callback_t callback, void* user_data);
ppdb_error_t ppdb_base_timer_stop(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_reset(ppdb_base_timer_t* timer);
void ppdb_base_timer_get_stats(ppdb_base_timer_t* timer, ppdb_base_timer_stats_t* stats);
bool ppdb_base_timer_is_active(ppdb_base_timer_t* timer);
uint64_t ppdb_base_timer_get_remaining(ppdb_base_timer_t* timer);
void ppdb_base_timer_process(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_set_interval(ppdb_base_timer_t* timer, uint64_t timeout_ms);
void ppdb_base_timer_clear_stats(ppdb_base_timer_t* timer);

// Async functions
ppdb_error_t ppdb_base_async_init(ppdb_base_async_loop_t** loop);
ppdb_error_t ppdb_base_async_cleanup(ppdb_base_async_loop_t* loop);
ppdb_error_t ppdb_base_async_submit(ppdb_base_async_loop_t* loop, ppdb_base_async_func_t func, void* arg);

// IO functions
ppdb_error_t ppdb_base_io_manager_create(ppdb_base_io_manager_t** manager);
ppdb_error_t ppdb_base_io_manager_destroy(ppdb_base_io_manager_t* manager);
ppdb_error_t ppdb_base_io_submit(ppdb_base_io_manager_t* manager, ppdb_base_io_func_t func, void* arg);

// Utility functions
bool ppdb_base_str_equal(const char* s1, const char* s2);
size_t ppdb_base_str_hash(const char* str);
uint64_t ppdb_base_get_time_us(void);
int ppdb_base_ptr_compare(const void* a, const void* b);
int ppdb_base_int_compare(const void* a, const void* b);
int ppdb_base_str_compare(const void* a, const void* b);
void ppdb_base_normalize_path(char* path);
bool ppdb_base_is_absolute_path(const char* path);
void ppdb_base_get_dirname(char* path);
void ppdb_base_get_basename(const char* path, char* basename, size_t size);
void ppdb_base_rand_init(uint64_t seed);
uint32_t ppdb_base_rand(void);
uint32_t ppdb_base_rand_range(uint32_t min, uint32_t max);
uint32_t ppdb_base_get_cpu_count(void);
size_t ppdb_base_get_page_size(void);
uint64_t ppdb_base_get_total_memory(void);
bool ppdb_base_is_power_of_two(size_t x);
size_t ppdb_base_align_size(size_t size, size_t alignment);
uint32_t ppdb_base_next_power_of_two(uint32_t x);
int ppdb_base_count_bits(uint32_t x);

// Spinlock functions
ppdb_error_t ppdb_base_spinlock_create(ppdb_base_spinlock_t** spinlock);
ppdb_error_t ppdb_base_spinlock_destroy(ppdb_base_spinlock_t* spinlock);
ppdb_error_t ppdb_base_spinlock_lock(ppdb_base_spinlock_t* spinlock);
ppdb_error_t ppdb_base_spinlock_unlock(ppdb_base_spinlock_t* spinlock);
ppdb_error_t ppdb_base_spinlock_trylock(ppdb_base_spinlock_t* spinlock);
void ppdb_base_spinlock_enable_stats(ppdb_base_spinlock_t* spinlock, bool enable);

// Async task structure
struct ppdb_base_async_handle_s {
    ppdb_base_async_func_t fn;
    void* arg;
    struct ppdb_base_async_handle_s* next;
    bool cancelled;
};

// Function declarations for async operations
ppdb_error_t ppdb_base_async_schedule(ppdb_base_t* base, ppdb_base_async_func_t fn, void* arg, ppdb_base_async_handle_t** handle);
void ppdb_base_async_cancel(ppdb_base_async_handle_t* handle);

#endif /* PPDB_BASE_H */