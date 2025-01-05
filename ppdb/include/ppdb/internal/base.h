#ifndef PPDB_INTERNAL_BASE_H
#define PPDB_INTERNAL_BASE_H

#include <cosmopolitan.h>
#include "../ppdb.h"

// Memory alignment
#define PPDB_ALIGNMENT 16

// Error codes
#define PPDB_ERR_CUSTOM_BASE 1000
#define PPDB_ERR_INVALID_PARAM (PPDB_ERR_CUSTOM_BASE + 1)

// Memory management
void* ppdb_aligned_alloc(size_t size);
void ppdb_aligned_free(void* ptr);

#define PPDB_ALIGNED_ALLOC(size) ppdb_aligned_alloc(size)
#define PPDB_ALIGNED_FREE(ptr) ppdb_aligned_free(ptr)

// Basic data types
typedef struct {
    uint8_t* data;
    size_t size;
} ppdb_key_t;

typedef struct {
    uint8_t* data;
    size_t size;
} ppdb_value_t;

// Memory pool block
typedef struct ppdb_mempool_block_s {
    struct ppdb_mempool_block_s* next;
    size_t size;
    size_t used;
    char data[];
} ppdb_mempool_block_t;

// Memory pool
typedef struct ppdb_mempool_s {
    ppdb_mempool_block_t* head;
    size_t block_size;
    size_t alignment;
} ppdb_mempool_t;

// Mutex
typedef struct ppdb_core_mutex_s {
    pthread_mutex_t mutex;
} ppdb_core_mutex_t;

// Context
typedef struct ppdb_context_s {
    ppdb_mempool_t* pool;
    uint32_t flags;
    void* user_data;
} ppdb_context_t;

// Cursor
typedef struct ppdb_cursor_s {
    ppdb_context_t* ctx;
    uint32_t flags;
    void* user_data;
    ppdb_core_mutex_t* mutex;
} ppdb_cursor_t;

// Base
typedef struct ppdb_base_s {
    ppdb_context_t* root_context;
    ppdb_mempool_t* pool;
    uint32_t flags;
} ppdb_base_t;

// Memory management functions
ppdb_error_t ppdb_mempool_create(ppdb_mempool_t** pool, size_t block_size, size_t alignment);
void ppdb_mempool_destroy(ppdb_mempool_t* pool);
void* ppdb_mempool_alloc(ppdb_mempool_t* pool, size_t size);

// Mutex functions
ppdb_error_t ppdb_core_mutex_create(ppdb_core_mutex_t** mutex);
void ppdb_core_mutex_destroy(ppdb_core_mutex_t* mutex);
ppdb_error_t ppdb_core_mutex_lock(ppdb_core_mutex_t* mutex);
ppdb_error_t ppdb_core_mutex_unlock(ppdb_core_mutex_t* mutex);

// Context functions
ppdb_error_t ppdb_context_create(ppdb_ctx_t* ctx_handle);
void ppdb_context_destroy(ppdb_ctx_t ctx_handle);
ppdb_context_t* ppdb_context_get(ppdb_ctx_t ctx_handle);

// Cursor functions
ppdb_error_t ppdb_cursor_create(ppdb_ctx_t ctx_handle, ppdb_cursor_t** cursor);
void ppdb_cursor_destroy(ppdb_cursor_t* cursor);
ppdb_error_t ppdb_cursor_next(ppdb_cursor_t* cursor, ppdb_data_t* key, ppdb_data_t* value);

// Base functions
ppdb_error_t ppdb_base_init(ppdb_base_t** base);
void ppdb_base_destroy(ppdb_base_t* base);

#endif // PPDB_INTERNAL_BASE_H