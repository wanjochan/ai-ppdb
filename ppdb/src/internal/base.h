#ifndef PPDB_INTERNAL_BASE_H_
#define PPDB_INTERNAL_BASE_H_

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>

//-----------------------------------------------------------------------------
// Error handling
//-----------------------------------------------------------------------------

// Base layer error codes
#define PPDB_BASE_ERR_START     (PPDB_ERROR_START + 0x100)  // Base: 0x1100-0x11FF
#define PPDB_BASE_ERR_MUTEX     (PPDB_BASE_ERR_START + 0x01)
#define PPDB_BASE_ERR_RWLOCK    (PPDB_BASE_ERR_START + 0x02)
#define PPDB_BASE_ERR_THREAD    (PPDB_BASE_ERR_START + 0x03)
#define PPDB_BASE_ERR_SYNC      (PPDB_BASE_ERR_START + 0x04)
#define PPDB_BASE_ERR_POOL      (PPDB_BASE_ERR_START + 0x05)
#define PPDB_BASE_ERR_MEMORY    (PPDB_BASE_ERR_START + 0x06)
#define PPDB_BASE_ERR_IO        (PPDB_BASE_ERR_START + 0x07)
#define PPDB_BASE_ERR_PARAM     (PPDB_BASE_ERR_START + 0x08)
#define PPDB_BASE_ERR_INVALID_STATE (PPDB_BASE_ERR_START + 0x09)
#define PPDB_BASE_ERR_CONFIG    (PPDB_BASE_ERR_START + 0x0A)
#define PPDB_ERR_BUFFER_FULL    (PPDB_BASE_ERR_START + 0x0B)
#define PPDB_ERR_INVALID_STATE  (PPDB_BASE_ERR_START + 0x0C)
#define PPDB_ERR_PARTIAL_WRITE  (PPDB_BASE_ERR_START + 0x0D)
#define PPDB_ERR_PARTIAL_READ   (PPDB_BASE_ERR_START + 0x0E)
#define PPDB_ERR_CONNECTION_CLOSED (PPDB_BASE_ERR_START + 0x0F)

// Error handling macros
#define PPDB_RETURN_IF_ERROR(expr) \
    do { \
        ppdb_error_t _err = (expr); \
        if (_err != PPDB_OK) return _err; \
    } while (0)

#define PPDB_GOTO_IF_ERROR(expr, label) \
    do { \
        ppdb_error_t _err = (expr); \
        if (_err != PPDB_OK) goto label; \
    } while (0)

#define PPDB_CHECK_NULL(ptr) \
    do { \
        if ((ptr) == NULL) return PPDB_BASE_ERR_PARAM; \
    } while (0)

#define PPDB_CHECK_PARAM(cond) \
    do { \
        if (!(cond)) return PPDB_BASE_ERR_PARAM; \
    } while (0)

// Error context structure
typedef struct ppdb_error_context_s {
    ppdb_error_t code;           // Error code
    const char* file;            // Source file where error occurred
    int line;                    // Line number where error occurred
    const char* func;            // Function name where error occurred
    char message[256];           // Optional error message
} ppdb_error_context_t;

// Error handling functions
void ppdb_error_init(void);
void ppdb_error_set_context(ppdb_error_context_t* ctx);
const ppdb_error_context_t* ppdb_error_get_context(void);
const char* ppdb_error_to_string(ppdb_error_t err);

//-----------------------------------------------------------------------------
// Base layer constants
//-----------------------------------------------------------------------------

#define PPDB_LOG_DEBUG 0
#define PPDB_LOG_INFO  1
#define PPDB_LOG_WARN  2
#define PPDB_LOG_ERROR 3
#define MAX_SKIPLIST_LEVEL 32

//-----------------------------------------------------------------------------
// Base layer types
//-----------------------------------------------------------------------------

// Forward declarations
typedef struct ppdb_base_s ppdb_base_t;
typedef struct ppdb_base_mutex_s ppdb_base_mutex_t;
typedef struct ppdb_base_spinlock_s ppdb_base_spinlock_t;
typedef struct ppdb_base_thread_s ppdb_base_thread_t;
typedef struct ppdb_base_rwlock_s ppdb_base_rwlock_t;
typedef struct ppdb_base_cond_s ppdb_base_cond_t;
typedef struct ppdb_base_file_s ppdb_base_file_t;
typedef struct ppdb_base_io_manager_s ppdb_base_io_manager_t;
typedef struct ppdb_base_timer_s ppdb_base_timer_t;
typedef struct ppdb_base_future_s ppdb_base_future_t;
typedef struct ppdb_base_event_loop_s ppdb_base_event_loop_t;
typedef struct ppdb_base_async_loop_s ppdb_base_async_loop_t;
typedef struct ppdb_base_async_handle_s ppdb_base_async_handle_t;
typedef struct ppdb_base_async_future_s ppdb_base_async_future_t;

// Memory pool structures
typedef struct ppdb_base_mempool_block_s {
    struct ppdb_base_mempool_block_s* next;
    size_t size;
    size_t used;
    char data[];
} ppdb_base_mempool_block_t;

typedef struct ppdb_base_mempool_s {
    ppdb_base_mempool_block_t* head;
    size_t block_size;
    size_t alignment;
} ppdb_base_mempool_t;

// Configuration types
typedef struct ppdb_base_config_s {
    size_t memory_limit;
    size_t thread_pool_size;
    bool thread_safe;
} ppdb_base_config_t;

typedef struct ppdb_base_stats_s {
    uint64_t total_allocs;
    uint64_t total_frees;
    uint64_t current_memory;
    uint64_t peak_memory;
} ppdb_base_stats_t;

// Context type
typedef struct ppdb_base_context_s {
    ppdb_base_mempool_t* pool;
    ppdb_base_t* base;
    ppdb_options_t options;
    uint32_t flags;
    void* user_data;
} ppdb_base_context_t;

// Cursor type
typedef struct ppdb_base_cursor_s {
    ppdb_base_context_t* ctx;
    void* internal;
} ppdb_base_cursor_t;

// Sync types
typedef struct ppdb_base_sync_config_s {
    bool thread_safe;
    uint32_t spin_count;
    uint32_t backoff_us;
} ppdb_base_sync_config_t;

typedef struct ppdb_base_sync_s {
    ppdb_base_mutex_t* mutex;
    uint32_t readers;
    bool writer;
    ppdb_base_sync_config_t config;
} ppdb_base_sync_t;

// Skip list types
typedef struct ppdb_base_skiplist_node_s ppdb_base_skiplist_node_t;
typedef struct ppdb_base_skiplist_s ppdb_base_skiplist_t;
typedef int (*ppdb_base_compare_func_t)(const void* a, const void* b);

// 原子计数器
typedef struct ppdb_base_counter_s {
    uint64_t value;
    ppdb_base_mutex_t* mutex;
} ppdb_base_counter_t;

// 回调函数类型
typedef void (*ppdb_base_thread_func_t)(void*);
typedef void (*ppdb_base_future_callback_t)(ppdb_base_future_t* future, void* data);
typedef void (*ppdb_base_timer_callback_t)(ppdb_base_timer_t* timer, void* data);
typedef void (*ppdb_base_io_callback_t)(ppdb_error_t status, void* data);
typedef void (*ppdb_base_async_cb)(ppdb_base_async_handle_t* handle, int status);

// Time functions
uint64_t ppdb_base_get_time_us(void);

// Thread operations
ppdb_error_t ppdb_base_thread_create(ppdb_base_thread_t** thread, ppdb_base_thread_func_t func, void* arg);
ppdb_error_t ppdb_base_thread_join(ppdb_base_thread_t* thread, void** retval);
ppdb_error_t ppdb_base_thread_detach(ppdb_base_thread_t* thread);
void ppdb_base_thread_destroy(ppdb_base_thread_t* thread);
int ppdb_base_thread_get_state(ppdb_base_thread_t* thread);
uint64_t ppdb_base_thread_get_wall_time(ppdb_base_thread_t* thread);
const char* ppdb_base_thread_get_error(ppdb_base_thread_t* thread);

// Mutex operations
ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex);
ppdb_error_t ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_trylock(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex);
void ppdb_base_mutex_enable_stats(ppdb_base_mutex_t* mutex, bool enable);
void ppdb_base_mutex_get_stats(ppdb_base_mutex_t* mutex, uint64_t* lock_count,
                              uint64_t* contention_count, uint64_t* total_wait_time_us,
                              uint64_t* max_wait_time_us);
const char* ppdb_base_mutex_get_error(ppdb_base_mutex_t* mutex);

// Spinlock operations
ppdb_error_t ppdb_base_spinlock_create(ppdb_base_spinlock_t** spinlock);
ppdb_error_t ppdb_base_spinlock_init(ppdb_base_spinlock_t* spinlock);
void ppdb_base_spinlock_destroy(ppdb_base_spinlock_t* spinlock);
ppdb_error_t ppdb_base_spinlock_lock(ppdb_base_spinlock_t* spinlock);
ppdb_error_t ppdb_base_spinlock_trylock(ppdb_base_spinlock_t* spinlock);
ppdb_error_t ppdb_base_spinlock_unlock(ppdb_base_spinlock_t* spinlock);
void ppdb_base_spinlock_enable_stats(ppdb_base_spinlock_t* spinlock, bool enable);
void ppdb_base_spinlock_set_spin_count(ppdb_base_spinlock_t* spinlock, uint32_t count);
void ppdb_base_spinlock_get_stats(ppdb_base_spinlock_t* spinlock, uint64_t* lock_count,
                                 uint64_t* contention_count, uint64_t* total_wait_time_us,
                                 uint64_t* max_wait_time_us);
const char* ppdb_base_spinlock_get_error(ppdb_base_spinlock_t* spinlock);

// Base initialization and cleanup
ppdb_error_t ppdb_base_init(ppdb_base_t** base, const ppdb_base_config_t* config);
void ppdb_base_destroy(ppdb_base_t* base);
void ppdb_base_get_stats(ppdb_base_t* base, ppdb_base_stats_t* stats);

// Memory management initialization
ppdb_error_t ppdb_base_memory_init(ppdb_base_t* base);
void ppdb_base_memory_cleanup(ppdb_base_t* base);

// Memory pool operations
ppdb_error_t ppdb_base_mempool_create(ppdb_base_mempool_t** pool, size_t block_size, size_t alignment);
void ppdb_base_mempool_destroy(ppdb_base_mempool_t* pool);
void* ppdb_base_mempool_alloc(ppdb_base_mempool_t* pool);
void ppdb_base_mempool_free(ppdb_base_mempool_t* pool, void* ptr);

// Aligned memory operations
void* ppdb_base_aligned_alloc(size_t alignment, size_t size);
void ppdb_base_aligned_free(void* ptr);

// Counter operations
ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter);
void ppdb_base_counter_destroy(ppdb_base_counter_t* counter);
uint64_t ppdb_base_counter_increment(ppdb_base_counter_t* counter);
uint64_t ppdb_base_counter_decrement(ppdb_base_counter_t* counter);
uint64_t ppdb_base_counter_get(ppdb_base_counter_t* counter);
void ppdb_base_counter_set(ppdb_base_counter_t* counter, uint64_t value);

// Skip list operations
ppdb_error_t ppdb_base_skiplist_create(ppdb_base_skiplist_t** list, ppdb_base_compare_func_t compare);
void ppdb_base_skiplist_destroy(ppdb_base_skiplist_t* list);
ppdb_error_t ppdb_base_skiplist_insert(ppdb_base_skiplist_t* list, const void* key, void* value);
ppdb_error_t ppdb_base_skiplist_find(ppdb_base_skiplist_t* list, const void* key, void** value);
ppdb_error_t ppdb_base_skiplist_remove(ppdb_base_skiplist_t* list, const void* key);
size_t ppdb_base_skiplist_size(ppdb_base_skiplist_t* list);

// Async operations
ppdb_error_t ppdb_base_async_loop_create(ppdb_base_async_loop_t** loop);
ppdb_error_t ppdb_base_async_loop_destroy(ppdb_base_async_loop_t* loop);
ppdb_error_t ppdb_base_async_loop_run(ppdb_base_async_loop_t* loop, int timeout_ms);

ppdb_error_t ppdb_base_async_handle_create(ppdb_base_async_loop_t* loop, int fd, ppdb_base_async_handle_t** handle);
ppdb_error_t ppdb_base_async_handle_destroy(ppdb_base_async_handle_t* handle);
ppdb_error_t ppdb_base_async_read(ppdb_base_async_handle_t* handle, void* buf, size_t len, ppdb_base_async_cb cb);
ppdb_error_t ppdb_base_async_write(ppdb_base_async_handle_t* handle, const void* buf, size_t len, ppdb_base_async_cb cb);

// Future operations
ppdb_error_t ppdb_base_future_create(ppdb_base_async_loop_t* loop, ppdb_base_async_future_t** future);
ppdb_error_t ppdb_base_future_destroy(ppdb_base_async_future_t* future);
ppdb_error_t ppdb_base_future_set_callback(ppdb_base_async_future_t* future, ppdb_base_async_cb cb, void* user_data);
ppdb_error_t ppdb_base_future_set_result(ppdb_base_async_future_t* future, void* result, size_t size);
ppdb_error_t ppdb_base_future_set_error(ppdb_base_async_future_t* future, ppdb_error_t error);
ppdb_error_t ppdb_base_future_wait(ppdb_base_async_future_t* future);
ppdb_error_t ppdb_base_future_wait_timeout(ppdb_base_async_future_t* future, uint32_t timeout_ms);
ppdb_error_t ppdb_base_future_is_ready(ppdb_base_async_future_t* future, bool* ready);
ppdb_error_t ppdb_base_future_get_result(ppdb_base_async_future_t* future, void* result, size_t size, size_t* actual_size);

// Timer statistics
typedef struct ppdb_base_timer_stats_s {
    uint64_t total_timeouts;      // Total number of timeouts
    uint64_t active_timers;       // Current active timers
    uint64_t total_resets;        // Total number of timer resets
    uint64_t total_cancels;       // Total number of timer cancels
    uint64_t peak_timers;         // Peak number of active timers
} ppdb_base_timer_stats_t;

// Timer structure
typedef struct ppdb_base_timer_s {
    ppdb_base_event_loop_t* loop;  // Event loop
    uint64_t timeout_us;          // Timeout in microseconds
    uint64_t next_timeout;        // Next timeout point
    bool repeat;                  // Whether timer repeats
    void* user_data;             // User data
    ppdb_base_timer_callback_t callback;  // Timer callback
    struct ppdb_base_timer_s* next;  // Next timer in list
    ppdb_base_timer_stats_t stats;  // Timer statistics
} ppdb_base_timer_t;

// Timer functions
ppdb_error_t ppdb_base_timer_create(ppdb_base_event_loop_t* loop,
                                   ppdb_base_timer_t** timer);
void ppdb_base_timer_destroy(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_start(ppdb_base_timer_t* timer,
                                  uint64_t timeout_ms,
                                  bool repeat,
                                  ppdb_base_timer_callback_t callback,
                                  void* user_data);
ppdb_error_t ppdb_base_timer_stop(ppdb_base_timer_t* timer);
ppdb_error_t ppdb_base_timer_reset(ppdb_base_timer_t* timer);

// Timer statistics functions
void ppdb_base_timer_get_stats(ppdb_base_timer_t* timer,
                              ppdb_base_timer_stats_t* stats);
void ppdb_base_timer_reset_stats(ppdb_base_timer_t* timer);

// Operating System Type Definitions
typedef enum ppdb_os_type {
    PPDB_OS_UNKNOWN = 0,
    PPDB_OS_WINDOWS,
    PPDB_OS_LINUX,
    PPDB_OS_MACOS,
    PPDB_OS_BSD
} ppdb_os_type_t;

// Operating System Detection Functions
ppdb_os_type_t ppdb_base_get_os_type(void);
const char* ppdb_base_get_os_name(void);
bool ppdb_base_is_windows(void);
bool ppdb_base_is_unix(void);

// Spinlock structure
struct ppdb_base_spinlock_s {
    _Atomic(int) lock;
    bool enable_stats;         // Runtime statistics control
    uint64_t lock_count;
    uint64_t contention_count;
    uint64_t total_wait_time_us;
    uint64_t max_wait_time_us;
    uint32_t spin_count;
};

// Memory pool statistics
typedef struct ppdb_base_mempool_stats_s {
    uint64_t total_allocs;        // Total number of allocations
    uint64_t total_frees;         // Total number of frees
    uint64_t current_blocks;      // Current number of blocks
    uint64_t peak_blocks;         // Peak number of blocks
    uint64_t current_bytes;       // Current bytes allocated
    uint64_t peak_bytes;          // Peak bytes allocated
    uint64_t total_bytes;         // Total bytes allocated
    uint64_t failed_allocs;       // Failed allocation attempts
} ppdb_base_mempool_stats_t;

// Memory pool operations
// ... existing code ...

// Memory pool statistics operations
void ppdb_base_mempool_get_stats(ppdb_base_mempool_t* pool, ppdb_base_mempool_stats_t* stats);
void ppdb_base_mempool_reset_stats(ppdb_base_mempool_t* pool);

// IO request types
#define PPDB_IO_READ  1
#define PPDB_IO_WRITE 2

// IO request state
#define PPDB_IO_PENDING   0
#define PPDB_IO_COMPLETE  1
#define PPDB_IO_ERROR     2

// IO Manager statistics
typedef struct ppdb_base_io_stats_s {
    uint64_t total_reads;          // Total read operations
    uint64_t total_writes;         // Total write operations
    uint64_t read_bytes;           // Total bytes read
    uint64_t write_bytes;          // Total bytes written
    uint64_t pending_ops;          // Current pending operations
    uint64_t completed_ops;        // Total completed operations
    uint64_t error_ops;           // Total operations with errors
    uint64_t total_wait_time_us;  // Total wait time in microseconds
} ppdb_base_io_stats_t;

// IO Request structure
typedef struct ppdb_base_io_request_s {
    uint32_t type;                // Request type (read/write)
    void* buffer;                 // Data buffer
    size_t size;                  // Buffer size
    uint64_t offset;             // File offset
    int fd;                      // File descriptor
    uint32_t state;              // Request state
    ppdb_error_t error;          // Error code if any
    uint64_t start_time;         // Request start time
    ppdb_base_io_callback_t callback;  // Completion callback
    void* callback_data;         // User data for callback
    struct ppdb_base_io_request_s* next;  // Next request in queue
} ppdb_base_io_request_t;

// IO Manager structure
typedef struct ppdb_base_io_manager_s {
    ppdb_base_mutex_t* mutex;           // Mutex for thread safety
    ppdb_base_thread_t* worker_thread;  // Worker thread
    ppdb_base_io_request_t* pending;    // Pending requests queue
    ppdb_base_io_request_t* completed;  // Completed requests queue
    bool running;                       // Manager running state
    ppdb_base_io_stats_t stats;        // IO statistics
} ppdb_base_io_manager_t;

// IO Manager functions
ppdb_error_t ppdb_base_io_manager_create(ppdb_base_io_manager_t** manager);
void ppdb_base_io_manager_destroy(ppdb_base_io_manager_t* manager);
ppdb_error_t ppdb_base_io_manager_start(ppdb_base_io_manager_t* manager);
ppdb_error_t ppdb_base_io_manager_stop(ppdb_base_io_manager_t* manager);

// Async IO operations
ppdb_error_t ppdb_base_io_read_async(ppdb_base_io_manager_t* manager, 
                                    int fd, void* buffer, size_t size, 
                                    uint64_t offset, ppdb_base_io_callback_t callback,
                                    void* callback_data);
ppdb_error_t ppdb_base_io_write_async(ppdb_base_io_manager_t* manager,
                                     int fd, const void* buffer, size_t size,
                                     uint64_t offset, ppdb_base_io_callback_t callback,
                                     void* callback_data);

// IO statistics operations
void ppdb_base_io_get_stats(ppdb_base_io_manager_t* manager, ppdb_base_io_stats_t* stats);
void ppdb_base_io_reset_stats(ppdb_base_io_manager_t* manager);

// Event types
#define PPDB_EVENT_NONE    0
#define PPDB_EVENT_READ    1
#define PPDB_EVENT_WRITE   2
#define PPDB_EVENT_ERROR   4
#define PPDB_EVENT_TIMEOUT 8

// Event loop statistics
typedef struct ppdb_base_event_stats_s {
    uint64_t total_events;         // Total events processed
    uint64_t active_handlers;      // Current active event handlers
    uint64_t total_timeouts;       // Total timeout events
    uint64_t total_errors;         // Total error events
    uint64_t total_wait_time_us;   // Total wait time in microseconds
} ppdb_base_event_stats_t;

// Event handler structure
typedef struct ppdb_base_event_handler_s {
    int fd;                        // File descriptor
    uint32_t events;              // Registered events
    void* data;                   // User data
    void (*callback)(struct ppdb_base_event_handler_s* handler, uint32_t events);
    struct ppdb_base_event_handler_s* next;  // Next handler in list
} ppdb_base_event_handler_t;

// Event loop structure
typedef struct ppdb_base_event_loop_s {
    ppdb_base_mutex_t* mutex;           // Mutex for thread safety
    ppdb_base_event_handler_t* handlers;  // List of event handlers
    ppdb_base_timer_t* timers;          // List of active timers
    bool running;                       // Loop running state
    ppdb_base_event_stats_t stats;      // Event statistics
    void* impl;                         // Platform-specific implementation
} ppdb_base_event_loop_t;

// Event loop functions
ppdb_error_t ppdb_base_event_loop_create(ppdb_base_event_loop_t** loop);
void ppdb_base_event_loop_destroy(ppdb_base_event_loop_t* loop);
ppdb_error_t ppdb_base_event_loop_run(ppdb_base_event_loop_t* loop, uint64_t timeout_ms);
ppdb_error_t ppdb_base_event_loop_stop(ppdb_base_event_loop_t* loop);

// Event handler functions
ppdb_error_t ppdb_base_event_handler_create(ppdb_base_event_loop_t* loop,
                                          ppdb_base_event_handler_t** handler,
                                          int fd, uint32_t events,
                                          void (*callback)(ppdb_base_event_handler_t*, uint32_t),
                                          void* data);
void ppdb_base_event_handler_destroy(ppdb_base_event_handler_t* handler);
ppdb_error_t ppdb_base_event_handler_modify(ppdb_base_event_handler_t* handler,
                                          uint32_t events);

// Event statistics functions
void ppdb_base_event_get_stats(ppdb_base_event_loop_t* loop,
                              ppdb_base_event_stats_t* stats);
void ppdb_base_event_reset_stats(ppdb_base_event_loop_t* loop);

#endif // PPDB_INTERNAL_BASE_H_