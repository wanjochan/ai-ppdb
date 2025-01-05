/*
 * base_memory.inc.c - Memory Management Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Core aligned memory allocation functions will be moved to base_mem.c
// Removing header guards since this will be merged

// Core aligned memory allocation functions remain the same
void* ppdb_base_aligned_alloc(size_t alignment, size_t size) {
    if (size == 0 || alignment == 0) return NULL;
    
    // Round up size to multiple of alignment
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
    
    // memalign is already the cosmopolitan equivalent
    void* ptr = memalign(alignment, aligned_size);
    if (!ptr) return NULL;
    
    return ptr;
}

void ppdb_base_aligned_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

// Memory pool management implementation
ppdb_error_t ppdb_base_mempool_create(ppdb_base_mempool_t** pool, size_t block_size, size_t alignment) {
    if (!pool) return PPDB_ERR_PARAM;
    if (*pool) return PPDB_ERR_PARAM;
    if (block_size == 0 || alignment == 0) return PPDB_ERR_PARAM;

    ppdb_base_mempool_t* p = (ppdb_base_mempool_t*)ppdb_base_aligned_alloc(alignment, sizeof(ppdb_base_mempool_t));
    if (!p) return PPDB_ERR_MEMORY;

    p->head = NULL;
    p->block_size = block_size;
    p->alignment = alignment;

    *pool = p;
    return PPDB_OK;
}

void ppdb_base_mempool_destroy(ppdb_base_mempool_t* pool) {
    if (!pool) return;

    ppdb_base_mempool_block_t* block = pool->head;
    while (block) {
        ppdb_base_mempool_block_t* next = block->next;
        ppdb_base_aligned_free(block);
        block = next;
    }

    ppdb_base_aligned_free(pool);
}

void* ppdb_base_mempool_alloc(ppdb_base_mempool_t* pool) {
    if (!pool) return NULL;

    // Find a block with enough space
    ppdb_base_mempool_block_t* block = pool->head;
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
    size_t header_size = offsetof(ppdb_base_mempool_block_t, data);
    size_t total_size = header_size + block_size;

    block = (ppdb_base_mempool_block_t*)ppdb_base_aligned_alloc(pool->alignment, total_size);
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

void ppdb_base_mempool_free(ppdb_base_mempool_t* pool, void* ptr) {
    if (!pool || !ptr) return;

    // Find the block containing ptr
    ppdb_base_mempool_block_t* block = pool->head;
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

// Memory management initialization
ppdb_error_t ppdb_base_memory_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_PARAM;

    // Create global memory pool
    ppdb_error_t err = ppdb_base_mempool_create(&base->global_pool, 4096, 16);
    if (err != PPDB_OK) return err;

    // Create memory mutex
    err = ppdb_base_mutex_create(&base->mem_mutex);
    if (err != PPDB_OK) {
        ppdb_base_mempool_destroy(base->global_pool);
        return err;
    }

    // Initialize statistics
    atomic_init(&base->stats.total_allocs, 0);
    atomic_init(&base->stats.total_frees, 0);
    atomic_init(&base->stats.current_memory, 0);
    atomic_init(&base->stats.peak_memory, 0);

    return PPDB_OK;
}

void ppdb_base_memory_cleanup(ppdb_base_t* base) {
    if (!base) return;

    if (base->mem_mutex) {
        ppdb_base_mutex_destroy(base->mem_mutex);
        base->mem_mutex = NULL;
    }

    if (base->global_pool) {
        ppdb_base_mempool_destroy(base->global_pool);
        base->global_pool = NULL;
    }
}