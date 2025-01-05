//-----------------------------------------------------------------------------
// Memory Management Implementation (Moved to base/base_mem.inc.c)
// This file should be deprecated after migration
//-----------------------------------------------------------------------------

// These functions are now moved to base layer and should be updated to use base_ prefix
// Keeping stubs here for backward compatibility

void* ppdb_engine_alloc(size_t size) {
    return ppdb_base_alloc(size);  // Call new base layer function
}

void ppdb_engine_free(void* ptr) {
    ppdb_base_free(ptr);  // Call new base layer function
}

void* ppdb_engine_calloc(size_t nmemb, size_t size) {
    return ppdb_base_calloc(nmemb, size);  // Call new base layer function
}

void* ppdb_engine_realloc(void* ptr, size_t size) {
    return ppdb_base_realloc(ptr, size);  // Call new base layer function
}

// ... existing code ...
