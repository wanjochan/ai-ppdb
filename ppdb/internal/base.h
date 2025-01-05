#ifndef PPDB_INTERNAL_BASE_H
#define PPDB_INTERNAL_BASE_H

#include "core.h"

//-----------------------------------------------------------------------------
// Context Management
//-----------------------------------------------------------------------------
typedef uint64_t ppdb_context_t;

ppdb_error_t ppdb_context_create(ppdb_context_t* ctx);
void ppdb_context_destroy(ppdb_context_t ctx);
ppdb_error_t ppdb_context_get_state(ppdb_context_t ctx, uint32_t* state);

//-----------------------------------------------------------------------------
// Safe Data Management
//-----------------------------------------------------------------------------
typedef struct ppdb_data {
    uint8_t inline_data[32];  // Small data optimization
    uint32_t size;
    uint32_t flags;
    void* extended_data;      // For data larger than inline_data
} ppdb_data_t;

ppdb_error_t ppdb_data_create(const void* data, size_t size, ppdb_data_t* out);
ppdb_error_t ppdb_data_destroy(ppdb_data_t* data);
ppdb_error_t ppdb_data_get(const ppdb_data_t* data, void* buf, size_t size, size_t* copied);
ppdb_error_t ppdb_data_size(const ppdb_data_t* data, size_t* size);

//-----------------------------------------------------------------------------
// Safe Iterator Management
//-----------------------------------------------------------------------------
typedef uint64_t ppdb_cursor_t;

ppdb_error_t ppdb_cursor_create(ppdb_context_t ctx, ppdb_cursor_t* cursor);
void ppdb_cursor_destroy(ppdb_cursor_t cursor);
ppdb_error_t ppdb_cursor_next(ppdb_cursor_t cursor, ppdb_data_t* key, ppdb_data_t* value);
ppdb_error_t ppdb_cursor_prev(ppdb_cursor_t cursor, ppdb_data_t* key, ppdb_data_t* value);
ppdb_error_t ppdb_cursor_seek(ppdb_cursor_t cursor, const ppdb_data_t* key);

//-----------------------------------------------------------------------------
// Batch Operations
//-----------------------------------------------------------------------------
typedef uint64_t ppdb_batch_t;

ppdb_error_t ppdb_batch_create(ppdb_context_t ctx, ppdb_batch_t* batch);
void ppdb_batch_destroy(ppdb_batch_t batch);
ppdb_error_t ppdb_batch_put(ppdb_batch_t batch, const ppdb_data_t* key, const ppdb_data_t* value);
ppdb_error_t ppdb_batch_delete(ppdb_batch_t batch, const ppdb_data_t* key);
ppdb_error_t ppdb_batch_commit(ppdb_batch_t batch);
ppdb_error_t ppdb_batch_clear(ppdb_batch_t batch);

#endif // PPDB_INTERNAL_BASE_H
