#ifndef PPDB_BASE_MEMORY_INC_C
#define PPDB_BASE_MEMORY_INC_C

// Core aligned memory allocation functions
void* ppdb_aligned_alloc(size_t alignment, size_t size) {
    if (size == 0 || alignment == 0) return NULL;
    
    // Round up size to multiple of alignment
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
    
    // Allocate memory with alignment
    void* ptr;
    if (posix_memalign(&ptr, alignment, aligned_size) != 0) {
        return NULL;
    }
    
    return ptr;
}

void ppdb_aligned_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

// Memory pool management implementation
ppdb_error_t ppdb_mempool_create(ppdb_mempool_t** pool, size_t block_size, size_t alignment) {
    if (!pool) return PPDB_ERR_NULL_POINTER;
    if (*pool) return PPDB_ERR_EXISTS;
    if (block_size == 0) return PPDB_ERR_INVALID_ARGUMENT;
    if (alignment == 0) return PPDB_ERR_INVALID_ARGUMENT;

    ppdb_mempool_t* p = (ppdb_mempool_t*)ppdb_aligned_alloc(alignment, sizeof(ppdb_mempool_t));
    if (!p) return PPDB_ERR_OUT_OF_MEMORY;

    p->head = NULL;
    p->block_size = block_size;
    p->alignment = alignment;

    *pool = p;
    return PPDB_OK;
}

void ppdb_mempool_destroy(ppdb_mempool_t* pool) {
    if (!pool) return;

    ppdb_mempool_block_t* block = pool->head;
    while (block) {
        ppdb_mempool_block_t* next = block->next;
        ppdb_aligned_free(block);
        block = next;
    }

    ppdb_aligned_free(pool);
}

void* ppdb_mempool_alloc(ppdb_mempool_t* pool) {
    if (!pool) return NULL;

    // Find a block with enough space
    ppdb_mempool_block_t* block = pool->head;
    while (block) {
        // Calculate aligned pointer
        uintptr_t current = (uintptr_t)block->data + block->used;
        uintptr_t aligned = (current + pool->alignment - 1) & ~(pool->alignment - 1);
        size_t padding = aligned - current;
        
        if (block->size - block->used >= padding + pool->alignment) {
            block->used = (size_t)(aligned - (uintptr_t)block->data) + pool->alignment;
            return (void*)aligned;
        }
        block = block->next;
    }

    // Need a new block
    size_t block_size = pool->block_size;
    size_t header_size = offsetof(ppdb_mempool_block_t, data);
    size_t total_size = header_size + block_size;

    block = (ppdb_mempool_block_t*)ppdb_aligned_alloc(pool->alignment, total_size);
    if (!block) return NULL;

    block->next = pool->head;
    block->size = block_size;
    
    // Calculate initial aligned pointer
    uintptr_t start = (uintptr_t)block->data;
    uintptr_t aligned = (start + pool->alignment - 1) & ~(pool->alignment - 1);
    block->used = (size_t)(aligned - start) + pool->alignment;
    pool->head = block;

    return (void*)aligned;
}

void ppdb_mempool_free(ppdb_mempool_t* pool, void* ptr) {
    if (!pool || !ptr) return;

    // Find the block containing ptr
    ppdb_mempool_block_t* block = pool->head;
    while (block) {
        if (ptr >= (void*)block->data && ptr < (void*)(block->data + block->size)) {
            // Found the block, but we don't support freeing individual allocations
            // Just mark the space as unused
            block->used = (char*)ptr - block->data;
            return;
        }
        block = block->next;
    }
}

#endif // PPDB_BASE_MEMORY_INC_C