/*
 * engine_io.inc.c - PPDB Engine I/O Operations
 *
 * This file contains the I/O operations for the PPDB engine layer,
 * handling file reading, writing and buffer management.
 *
 * Dependencies:
 * - ppdb/base/io.h
 * - ppdb/base/error.h
 */

#include "ppdb/base/io.h"
#include "ppdb/base/error.h"

// ... existing code ...

// Rename from read_page to ppdb_engine_read_page
static int ppdb_engine_read_page(PPDBFile *file, PageId page_id, char *buffer) {
    if (!file || !buffer) {
        return PPDB_ERROR_INVALID_ARGUMENT;
    }
    
    // ... existing implementation ...
}

// Rename from write_page to ppdb_engine_write_page
static int ppdb_engine_write_page(PPDBFile *file, PageId page_id, const char *buffer) {
    if (!file || !buffer) {
        return PPDB_ERROR_INVALID_ARGUMENT;
    }

    // ... existing implementation ...
}

// Rename from flush_buffer to ppdb_engine_flush_buffer
static int ppdb_engine_flush_buffer(PPDBFile *file) {
    if (!file) {
        return PPDB_ERROR_INVALID_ARGUMENT;
    }

    // ... existing implementation ...
}

// ... existing code ...