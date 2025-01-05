#ifndef PPDB_INTERNAL_BASE_H_
#define PPDB_INTERNAL_BASE_H_

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>

// Base layer constants
#define PPDB_LOG_DEBUG 0
#define PPDB_LOG_INFO  1
#define PPDB_LOG_WARN  2
#define PPDB_LOG_ERROR 3
#define MAX_SKIPLIST_LEVEL 32

// Extended base types
typedef struct ppdb_base_mutex_s ppdb_base_mutex_t;
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

// Base layer operations
ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex);
void ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex);

// ... existing rwlock operations with updated ppdb_base_ prefix ...

// Memory management
void* ppdb_base_aligned_alloc(size_t alignment, size_t size);
void ppdb_base_aligned_free(void* ptr);

// ... existing memory pool operations with updated ppdb_base_ prefix ...

// ... existing context operations with updated ppdb_base_ prefix ...

// ... existing cursor operations with updated ppdb_base_ prefix ...

// ... existing sync operations with updated ppdb_base_ prefix ...

// ... existing logging operations with updated ppdb_base_ prefix ...

// ... existing thread operations with updated ppdb_base_ prefix ...

// ... existing condition variable operations with updated ppdb_base_ prefix ...

// ... existing file IO operations with updated ppdb_base_ prefix ...

// ... existing IO manager operations with updated ppdb_base_ prefix ...

#endif // PPDB_INTERNAL_BASE_H_