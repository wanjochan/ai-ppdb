#ifndef PPDB_INTERNAL_BASE_H
#define PPDB_INTERNAL_BASE_H

#include "core.h"

//-----------------------------------------------------------------------------
// Context Management Internal
//-----------------------------------------------------------------------------
#define PPDB_MAX_CONTEXTS 1024

typedef struct ppdb_context_pool_entry {
    ppdb_core_mutex_t* mutex;
    uint32_t state;
    bool used;
} ppdb_context_pool_entry_t;

//-----------------------------------------------------------------------------
// Safe Data Management Internal
//-----------------------------------------------------------------------------
typedef struct ppdb_data_internal {
    uint8_t inline_data[32];  // Small data optimization
    uint32_t size;
    uint32_t flags;
    void* extended_data;  // For data larger than inline buffer
} ppdb_data_internal_t;

//-----------------------------------------------------------------------------
// Cursor Management Internal
//-----------------------------------------------------------------------------
#define PPDB_MAX_CURSORS 1024

typedef struct ppdb_cursor_pool_entry {
    ppdb_core_mutex_t* mutex;
    ppdb_context_t ctx;
    bool used;
} ppdb_cursor_pool_entry_t;

//-----------------------------------------------------------------------------
// Batch Management Internal
//-----------------------------------------------------------------------------
#define PPDB_MAX_BATCHES 1024

typedef struct ppdb_batch_pool_entry {
    ppdb_core_mutex_t* mutex;
    ppdb_context_t ctx;
    bool used;
} ppdb_batch_pool_entry_t;

#endif // PPDB_INTERNAL_BASE_H
