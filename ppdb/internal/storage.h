#ifndef PPDB_INTERNAL_STORAGE_H
#define PPDB_INTERNAL_STORAGE_H

#include "base.h"

//-----------------------------------------------------------------------------
// Storage Engine Types
//-----------------------------------------------------------------------------
typedef enum ppdb_storage_type {
    PPDB_STORAGE_MEMTABLE = 1,
    PPDB_STORAGE_SSTABLE = 2,
    PPDB_STORAGE_WAL = 3
} ppdb_storage_type_t;

//-----------------------------------------------------------------------------
// Table Operations
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_table_create(ppdb_context_t ctx, const char* name);
ppdb_error_t ppdb_table_drop(ppdb_context_t ctx, const char* name);
ppdb_error_t ppdb_table_open(ppdb_context_t ctx, const char* name);
ppdb_error_t ppdb_table_close(ppdb_context_t ctx);

//-----------------------------------------------------------------------------
// Data Operations
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_storage_put(ppdb_context_t ctx, const ppdb_data_t* key, const ppdb_data_t* value);
ppdb_error_t ppdb_storage_get(ppdb_context_t ctx, const ppdb_data_t* key, ppdb_data_t* value);
ppdb_error_t ppdb_storage_delete(ppdb_context_t ctx, const ppdb_data_t* key);

//-----------------------------------------------------------------------------
// Scan Operations
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_storage_scan(ppdb_context_t ctx, ppdb_cursor_t* cursor);
ppdb_error_t ppdb_storage_scan_range(ppdb_context_t ctx, 
                                    const ppdb_data_t* start_key,
                                    const ppdb_data_t* end_key,
                                    ppdb_cursor_t* cursor);

//-----------------------------------------------------------------------------
// Maintenance Operations
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_storage_compact(ppdb_context_t ctx);
ppdb_error_t ppdb_storage_flush(ppdb_context_t ctx);
ppdb_error_t ppdb_storage_checkpoint(ppdb_context_t ctx);

#endif // PPDB_INTERNAL_STORAGE_H
