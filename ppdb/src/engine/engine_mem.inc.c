//-----------------------------------------------------------------------------
// Memory Management Implementation
//-----------------------------------------------------------------------------

void* ppdb_core_alloc(size_t size) {
    if (size == 0) return NULL;
    return aligned_alloc(PPDB_ALIGNMENT, (size + PPDB_ALIGNMENT - 1) & ~(PPDB_ALIGNMENT - 1));
}

void ppdb_core_free(void* ptr) {
    if (ptr) free(ptr);
}

void* ppdb_core_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;
    
    size_t total_size = nmemb * size;
    // Check for overflow
    if (size != total_size / nmemb) return NULL;
    
    void* ptr = ppdb_core_alloc(total_size);
    if (ptr) memset(ptr, 0, total_size);
    return ptr;
}

void* ppdb_core_realloc(void* ptr, size_t size) {
    if (size == 0) {
        ppdb_core_free(ptr);
        return NULL;
    }
    
    size_t aligned_size = (size + PPDB_ALIGNMENT - 1) & ~(PPDB_ALIGNMENT - 1);
    return realloc(ptr, aligned_size);
}
