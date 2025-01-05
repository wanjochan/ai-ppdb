//-----------------------------------------------------------------------------
// DEPRECATED: Memory Management Implementation 
// This file has been migrated to base/base_mem.inc.c
// These stubs are maintained only for backward compatibility
// TODO: Remove this file once all callers have migrated to base layer
//-----------------------------------------------------------------------------

#include "base/base_mem.h"

// Forward declarations of memory management functions
// Each function simply forwards calls to the base layer implementation

void* ppdb_engine_alloc(size_t size) {
    return ppdb_base_alloc(size);
}

void ppdb_engine_free(void* ptr) {
    ppdb_base_free(ptr);
}

void* ppdb_engine_calloc(size_t nmemb, size_t size) {
    return ppdb_base_calloc(nmemb, size);
}

void* ppdb_engine_realloc(void* ptr, size_t size) {
    return ppdb_base_realloc(ptr, size);
}

// ... existing code ...
