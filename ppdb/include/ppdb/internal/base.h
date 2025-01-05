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

// Base types (moved from engine)
typedef struct ppdb_mutex_s ppdb_mutex_t;
typedef struct ppdb_rwlock_s ppdb_rwlock_t;
typedef struct ppdb_cond_s ppdb_cond_t;

// Memory pool structures
typedef struct ppdb_mempool_block_s {
    struct ppdb_mempool_block_s* next;
    size_t size;
    size_t used;
    char data[];
} ppdb_mempool_block_t;

typedef struct ppdb_mempool_s {
    ppdb_mempool_block_t* head;
    size_t block_size;
    size_t alignment;
} ppdb_mempool_t;

// Context type
typedef struct ppdb_context_s {
    ppdb_mempool_t* pool;
    ppdb_base_t* base;
    ppdb_options_t options;
    uint32_t flags;
    void* user_data;
} ppdb_context_t;

// Cursor type
typedef struct ppdb_cursor_s {
    ppdb_context_t* ctx;
    void* internal;
} ppdb_cursor_t;

// Sync types with updated mutex reference
typedef struct ppdb_sync_config_s {
    bool thread_safe;
    uint32_t spin_count;
    uint32_t backoff_us;
} ppdb_sync_config_t;

typedef struct ppdb_sync_s {
    ppdb_mutex_t* mutex;  // Updated to use base mutex type
    uint32_t readers;
    bool writer;
    ppdb_sync_config_t config;
} ppdb_sync_t;

// Base layer operations (moved from engine)
ppdb_error_t ppdb_mutex_create(ppdb_mutex_t** mutex);
void ppdb_mutex_destroy(ppdb_mutex_t* mutex);
ppdb_error_t ppdb_mutex_lock(ppdb_mutex_t* mutex);
ppdb_error_t ppdb_mutex_unlock(ppdb_mutex_t* mutex);

ppdb_error_t ppdb_rwlock_create(ppdb_rwlock_t** rwlock);
void ppdb_rwlock_destroy(ppdb_rwlock_t* rwlock);
ppdb_error_t ppdb_rwlock_rdlock(ppdb_rwlock_t* rwlock);
ppdb_error_t ppdb_rwlock_wrlock(ppdb_rwlock_t* rwlock);
ppdb_error_t ppdb_rwlock_unlock(ppdb_rwlock_t* rwlock);

// Memory management
void* ppdb_aligned_alloc(size_t alignment, size_t size);
void ppdb_aligned_free(void* ptr);

// Memory pool operations
ppdb_error_t ppdb_mempool_create(ppdb_mempool_t** pool, size_t block_size, size_t alignment);
void* ppdb_mempool_alloc(ppdb_mempool_t* pool);
void ppdb_mempool_free(ppdb_mempool_t* pool, void* ptr);
void ppdb_mempool_destroy(ppdb_mempool_t* pool);

// Context operations
ppdb_error_t ppdb_context_create(ppdb_context_t** ctx);
void ppdb_context_destroy(ppdb_context_t* ctx);
ppdb_context_t* ppdb_context_get(ppdb_ctx_t ctx_handle);

// Cursor operations
ppdb_error_t ppdb_cursor_create(ppdb_ctx_t ctx_handle, ppdb_cursor_t** cursor);
void ppdb_cursor_destroy(ppdb_cursor_t* cursor);
ppdb_error_t ppdb_cursor_next(ppdb_cursor_t* cursor, ppdb_data_t* key, ppdb_data_t* value);

// Sync operations
ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, const ppdb_sync_config_t* config);
void ppdb_sync_destroy(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_read_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_write_lock(ppdb_sync_t* sync);

// Logging
ppdb_error_t ppdb_log_init(const char* filename, int level, bool thread_safe);
void ppdb_log_close(void);
void ppdb_log_debug(const char* format, ...);
void ppdb_log_info(const char* format, ...);
void ppdb_log_warn(const char* format, ...);
void ppdb_log_error(const char* format, ...);

#endif // PPDB_INTERNAL_BASE_H_