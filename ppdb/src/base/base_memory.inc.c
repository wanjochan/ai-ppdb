/*
 * base_memory.inc.c - Memory Management Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Add memory pool statistics structure
typedef struct ppdb_mempool_stats {
    atomic_size_t total_allocs;
    atomic_size_t total_frees; 
    atomic_size_t current_memory;
    atomic_size_t peak_memory;
} ppdb_mempool_stats_t;

// Core aligned memory allocation functions
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

// Updated memory pool structure
typedef struct ppdb_base_mempool {
    ppdb_base_mempool_block_t* head;
    size_t block_size;
    size_t alignment;
    ppdb_mempool_stats_t stats;  // Add statistics tracking
} ppdb_base_mempool_t;

ppdb_error_t ppdb_base_mempool_create(ppdb_base_mempool_t** pool, size_t block_size, size_t alignment) {
    if (!pool) return PPDB_BASE_ERR_PARAM;
    if (*pool) return PPDB_BASE_ERR_PARAM;
    if (block_size == 0 || alignment == 0) return PPDB_BASE_ERR_PARAM;

    ppdb_base_mempool_t* p = (ppdb_base_mempool_t*)malloc(sizeof(ppdb_base_mempool_t));
    if (!p) return PPDB_BASE_ERR_MEMORY;

    p->head = NULL;
    p->block_size = block_size;
    p->alignment = alignment;
    
    // Initialize statistics
    atomic_init(&p->stats.total_allocs, 0);
    atomic_init(&p->stats.total_frees, 0);
    atomic_init(&p->stats.current_memory, 0);
    atomic_init(&p->stats.peak_memory, 0);

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

    // Update statistics
    atomic_fetch_add_explicit(&pool->stats.total_allocs, 1, memory_order_relaxed);
    size_t current = atomic_fetch_add_explicit(&pool->stats.current_memory, 
                                             pool->alignment, 
                                             memory_order_relaxed);
    
    // Update peak memory if needed
    size_t peak;
    do {
        peak = atomic_load_explicit(&pool->stats.peak_memory, memory_order_relaxed);
        if (current <= peak) break;
    } while (!atomic_compare_exchange_weak_explicit(&pool->stats.peak_memory,
                                                  &peak, current,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed));

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
            break;
        }
        block = block->next;
    }

    // Update statistics
    atomic_fetch_add_explicit(&pool->stats.total_frees, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&pool->stats.current_memory, 
                            pool->alignment,
                            memory_order_relaxed);
}

// Memory management functions
ppdb_error_t ppdb_base_memory_init(ppdb_base_t* base) {
    if (!base) return PPDB_BASE_ERR_PARAM;

    // Create global memory pool
    base->global_pool = (ppdb_base_mempool_t*)malloc(sizeof(ppdb_base_mempool_t));
    if (!base->global_pool) return PPDB_BASE_ERR_MEMORY;

    memset(base->global_pool, 0, sizeof(ppdb_base_mempool_t));

    // Create mutex for memory operations
    ppdb_error_t err = ppdb_base_mutex_create(&base->mem_mutex);
    if (err != PPDB_OK) {
        free(base->global_pool);
        base->global_pool = NULL;
        return err;
    }

    return PPDB_OK;
}

void ppdb_base_memory_cleanup(ppdb_base_t* base) {
    if (!base) return;

    if (base->mem_mutex) {
        ppdb_base_mutex_destroy(base->mem_mutex);
        base->mem_mutex = NULL;
    }

    if (base->global_pool) {
        ppdb_base_mempool_block_t* block = base->global_pool->head;
        while (block) {
            ppdb_base_mempool_block_t* next = block->next;
            free(block);
            block = next;
        }
        free(base->global_pool);
        base->global_pool = NULL;
    }
}

void ppdb_base_memory_get_stats(ppdb_base_t* base, ppdb_base_stats_t* stats) {
    if (!base || !stats || !base->global_pool) return;

    ppdb_mempool_stats_t* pool_stats = &base->global_pool->stats;
    
    stats->total_allocs = atomic_load_explicit(&pool_stats->total_allocs, memory_order_relaxed);
    stats->total_frees = atomic_load_explicit(&pool_stats->total_frees, memory_order_relaxed);
    stats->current_memory = atomic_load_explicit(&pool_stats->current_memory, memory_order_relaxed);
    stats->peak_memory = atomic_load_explicit(&pool_stats->peak_memory, memory_order_relaxed);
}