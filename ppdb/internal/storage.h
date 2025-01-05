#ifndef PPDB_INTERNAL_STORAGE_H
#define PPDB_INTERNAL_STORAGE_H

#include "core.h"

//-----------------------------------------------------------------------------
// Storage Types
//-----------------------------------------------------------------------------
typedef struct ppdb_storage_block {
    uint64_t offset;
    uint32_t size;
    uint32_t flags;
    void* data;
    struct ppdb_storage_block* next;
} ppdb_storage_block_t;

typedef struct ppdb_storage_cache {
    ppdb_core_mutex_t* mutex;
    uint32_t size;
    uint32_t capacity;
    ppdb_storage_block_t* blocks;
} ppdb_storage_cache_t;

typedef struct ppdb_storage_internal {
    ppdb_core_file_t* file;
    ppdb_core_mutex_t* mutex;
    ppdb_storage_cache_t* cache;
    ppdb_storage_config_t config;
} ppdb_storage_internal_t;

//-----------------------------------------------------------------------------
// Internal Functions
//-----------------------------------------------------------------------------
// Cache Management
ppdb_error_t ppdb_storage_cache_create(uint32_t capacity, ppdb_storage_cache_t** cache);
void ppdb_storage_cache_destroy(ppdb_storage_cache_t* cache);
ppdb_error_t ppdb_storage_cache_get(ppdb_storage_cache_t* cache, uint64_t offset, ppdb_storage_block_t** block);
ppdb_error_t ppdb_storage_cache_put(ppdb_storage_cache_t* cache, ppdb_storage_block_t* block);
void ppdb_storage_cache_remove(ppdb_storage_cache_t* cache, uint64_t offset);

// Block Management
ppdb_error_t ppdb_storage_block_create(uint64_t offset, uint32_t size, ppdb_storage_block_t** block);
void ppdb_storage_block_destroy(ppdb_storage_block_t* block);
ppdb_error_t ppdb_storage_block_read(ppdb_storage_internal_t* storage, ppdb_storage_block_t* block);
ppdb_error_t ppdb_storage_block_write(ppdb_storage_internal_t* storage, ppdb_storage_block_t* block);

#endif // PPDB_INTERNAL_STORAGE_H
