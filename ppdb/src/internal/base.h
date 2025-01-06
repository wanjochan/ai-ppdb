#ifndef PPDB_INTERNAL_BASE_H_
#define PPDB_INTERNAL_BASE_H_

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>

//-----------------------------------------------------------------------------
// Error handling
//-----------------------------------------------------------------------------

// Module error ranges
#define PPDB_BASE_ERR_START    (PPDB_ERROR_START + 0x100)  // Base: 0x1100-0x11FF
#define PPDB_ENGINE_ERR_START  (PPDB_ERROR_START + 0x200)  // Engine: 0x1200-0x12FF

// Base layer error codes (0x1100-0x11FF)
#define PPDB_BASE_ERR_MUTEX    (PPDB_BASE_ERR_START + 0x01)
#define PPDB_BASE_ERR_RWLOCK   (PPDB_BASE_ERR_START + 0x02)
#define PPDB_BASE_ERR_THREAD   (PPDB_BASE_ERR_START + 0x03)
#define PPDB_BASE_ERR_SYNC     (PPDB_BASE_ERR_START + 0x04)
#define PPDB_BASE_ERR_POOL     (PPDB_BASE_ERR_START + 0x05)
#define PPDB_BASE_ERR_MEMORY   (PPDB_BASE_ERR_START + 0x06)
#define PPDB_BASE_ERR_IO       (PPDB_BASE_ERR_START + 0x07)
#define PPDB_BASE_ERR_PARAM    (PPDB_BASE_ERR_START + 0x08)
#define PPDB_ERR_INVALID_STATE (PPDB_BASE_ERR_START + 0x09)

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

// Extended base types
struct ppdb_base_mutex_s {
    pthread_mutex_t mutex;
    bool initialized;
};
typedef struct ppdb_base_mutex_s ppdb_base_mutex_t;

// 自旋锁类型
typedef struct ppdb_base_spinlock_s {
    atomic_flag flag;
    bool initialized;
} ppdb_base_spinlock_t;

typedef struct ppdb_base_rwlock_s ppdb_base_rwlock_t;
typedef struct ppdb_base_cond_s ppdb_base_cond_t;
typedef struct ppdb_base_thread_s ppdb_base_thread_t;
typedef struct ppdb_base_file_s ppdb_base_file_t;
typedef struct ppdb_base_io_manager_s ppdb_base_io_manager_t;

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

// IO structures
typedef struct ppdb_base_io_request_s {
    void* buffer;
    size_t size;
    uint64_t offset;
    bool is_write;
    void* callback_data;
} ppdb_base_io_request_t;

typedef struct ppdb_base_io_stats_s {
    uint64_t reads;
    uint64_t writes;
    uint64_t read_bytes;
    uint64_t write_bytes;
} ppdb_base_io_stats_t;

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

// Sync types with updated mutex reference
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
typedef int (*ppdb_base_compare_func_t)(void* a, void* b);

// 原子计数器
typedef struct ppdb_base_counter_s {
    uint64_t value;
    ppdb_base_mutex_t* mutex;
} ppdb_base_counter_t;

// 线程函数类型
typedef void* (*ppdb_base_thread_func_t)(void*);

// Thread operations
ppdb_error_t ppdb_base_thread_create(ppdb_base_thread_t** thread, ppdb_base_thread_func_t func, void* arg);
ppdb_error_t ppdb_base_thread_join(ppdb_base_thread_t* thread, void** retval);
void ppdb_base_thread_destroy(ppdb_base_thread_t* thread);

//-----------------------------------------------------------------------------
// Base layer functions
//-----------------------------------------------------------------------------

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

// Mutex operations
ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex);
void ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex);

// 自旋锁操作
ppdb_error_t ppdb_base_spinlock_create(ppdb_base_spinlock_t** spinlock);
void ppdb_base_spinlock_destroy(ppdb_base_spinlock_t* spinlock);
ppdb_error_t ppdb_base_spinlock_lock(ppdb_base_spinlock_t* spinlock);
ppdb_error_t ppdb_base_spinlock_unlock(ppdb_base_spinlock_t* spinlock);

// Sync initialization
ppdb_error_t ppdb_base_sync_init(ppdb_base_t* base);
void ppdb_base_sync_cleanup(ppdb_base_t* base);

// Utils initialization
ppdb_error_t ppdb_base_utils_init(ppdb_base_t* base);
void ppdb_base_utils_cleanup(ppdb_base_t* base);

// Skip list operations
ppdb_error_t ppdb_base_skiplist_create(ppdb_base_skiplist_t** list, ppdb_base_compare_func_t compare);
void ppdb_base_skiplist_destroy(ppdb_base_skiplist_t* list);
ppdb_error_t ppdb_base_skiplist_insert(ppdb_base_skiplist_t* list, void* key, void* value);
void* ppdb_base_skiplist_find(ppdb_base_skiplist_t* list, void* key);
ppdb_error_t ppdb_base_skiplist_remove(ppdb_base_skiplist_t* list, void* key);
size_t ppdb_base_skiplist_size(ppdb_base_skiplist_t* list);

// 原子计数器操作
ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter);
void ppdb_base_counter_destroy(ppdb_base_counter_t* counter);
uint64_t ppdb_base_counter_increment(ppdb_base_counter_t* counter);
uint64_t ppdb_base_counter_decrement(ppdb_base_counter_t* counter);
uint64_t ppdb_base_counter_get(ppdb_base_counter_t* counter);
void ppdb_base_counter_set(ppdb_base_counter_t* counter, uint64_t value);

#endif // PPDB_INTERNAL_BASE_H_