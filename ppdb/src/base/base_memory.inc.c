/*
 * base_memory.inc.c - Memory Management Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Memory management initialization
ppdb_error_t ppdb_base_memory_init(void) {
    return PPDB_OK;
}

// Memory management cleanup
void ppdb_base_memory_cleanup(void) {
    // Nothing to clean up in the basic implementation
}

// Memory allocation with alignment
void* ppdb_base_aligned_alloc(size_t alignment, size_t size) {
    if (alignment == 0 || size == 0) return NULL;
    if ((alignment & (alignment - 1)) != 0) return NULL;  // Must be power of 2

#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
#endif
}

// Free aligned memory
void ppdb_base_aligned_free(void* ptr) {
    if (!ptr) return;

#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// Create memory pool
ppdb_error_t ppdb_base_mempool_create(ppdb_base_mempool_t** pool, size_t block_size, size_t alignment) {
    if (!pool || block_size == 0) {
        return PPDB_BASE_ERR_PARAM;
    }

    // Ensure alignment is a power of 2
    if ((alignment & (alignment - 1)) != 0) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_mempool_t* new_pool = (ppdb_base_mempool_t*)malloc(sizeof(ppdb_base_mempool_t));
    if (!new_pool) {
        return PPDB_BASE_ERR_MEMORY;
    }

    new_pool->head = NULL;
    new_pool->block_size = block_size;
    new_pool->alignment = alignment;

    *pool = new_pool;
    return PPDB_OK;
}

// Destroy memory pool
ppdb_error_t ppdb_base_mempool_destroy(ppdb_base_mempool_t* pool) {
    if (!pool) return PPDB_BASE_ERR_PARAM;

    // Free all blocks
    ppdb_base_mempool_block_t* block = pool->head;
    while (block) {
        ppdb_base_mempool_block_t* next = block->next;
        free(block->data);
        free(block);
        block = next;
    }

    // Free pool
    free(pool);
    return PPDB_OK;
}

// Allocate memory from pool
void* ppdb_base_mempool_alloc(ppdb_base_mempool_t* pool, size_t size) {
    if (!pool || size == 0) {
        return NULL;
    }

    // Align size to pool alignment
    size = (size + pool->alignment - 1) & ~(pool->alignment - 1);

    // Find a block with enough space
    ppdb_base_mempool_block_t* block = pool->head;
    while (block) {
        // Align the current position
        size_t aligned_used = (block->used + pool->alignment - 1) & ~(pool->alignment - 1);
        if (block->size - aligned_used >= size) {
            void* ptr = (char*)block->data + aligned_used;
            block->used = aligned_used + size;
            return ptr;
        }
        block = block->next;
    }

    // Create a new block
    size_t block_size = size > pool->block_size ? size : pool->block_size;
    
    // Allocate block structure
    block = (ppdb_base_mempool_block_t*)malloc(sizeof(ppdb_base_mempool_block_t));
    if (!block) {
        return NULL;
    }

    // Allocate aligned memory for data
    block->data = ppdb_base_aligned_alloc(pool->alignment, block_size);
    if (!block->data) {
        free(block);
        return NULL;
    }

    block->next = pool->head;
    block->size = block_size;
    block->used = size;
    pool->head = block;

    return block->data;
}

// Free memory pool block
void ppdb_base_mempool_free(ppdb_base_mempool_t* pool, void* ptr) {
    if (!pool || !ptr) return;

    // In this simple implementation, we don't actually free individual allocations
    // Memory is only freed when the pool is destroyed
}

// Memory utilities
bool ppdb_base_is_power_of_two(size_t x) {
    return x && !(x & (x - 1));
}

size_t ppdb_base_align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

uint32_t ppdb_base_next_power_of_two(uint32_t x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

int ppdb_base_count_bits(uint32_t x) {
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}