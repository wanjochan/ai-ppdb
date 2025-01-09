/*
 * base_memory.inc.c - Memory Management Implementation
 *
 * This file contains:
 * 1. Memory allocation and deallocation
 * 2. Memory statistics tracking
 * 3. Memory limit enforcement
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Utility macros
#define IS_POWER_OF_TWO(x) ((x) && !((x) & ((x) - 1)))

// Memory statistics
static struct {
    _Atomic(size_t) total_allocated;
    _Atomic(size_t) total_freed;
    _Atomic(size_t) current_usage;
    _Atomic(size_t) peak_usage;
    size_t memory_limit;
    ppdb_base_mutex_t* stats_mutex;
} memory_stats = {0};

// Update memory statistics
static void update_memory_stats(size_t size, bool is_alloc) {
    if (is_alloc) {
        atomic_fetch_add(&memory_stats.total_allocated, size);
        size_t current = atomic_fetch_add(&memory_stats.current_usage, size) + size;
        size_t peak = atomic_load(&memory_stats.peak_usage);
        while (current > peak) {
            if (atomic_compare_exchange_weak(&memory_stats.peak_usage, &peak, current)) {
                break;
            }
            peak = atomic_load(&memory_stats.peak_usage);
        }
    } else {
        atomic_fetch_add(&memory_stats.total_freed, size);
        atomic_fetch_sub(&memory_stats.current_usage, size);
    }
}

// Initialize memory management
ppdb_error_t ppdb_base_memory_init(void) {
    if (memory_stats.stats_mutex) return PPDB_OK;
    
    return ppdb_base_mutex_create(&memory_stats.stats_mutex);
}

// Cleanup memory management
void ppdb_base_memory_cleanup(void) {
    if (memory_stats.stats_mutex) {
        ppdb_base_mutex_destroy(memory_stats.stats_mutex);
        memory_stats.stats_mutex = NULL;
    }
}

// Memory pool implementation
static ppdb_error_t mempool_create_block(ppdb_base_mempool_t* pool) {
    ppdb_base_mempool_block_t* block = malloc(sizeof(ppdb_base_mempool_block_t));
    if (!block) {
        return ppdb_error_set(PPDB_BASE_ERR_MEMORY, __FILE__, __LINE__, __func__,
                             "Failed to allocate memory pool block");
    }
    
    block->data = aligned_alloc(pool->alignment, pool->block_size);
    if (!block->data) {
        free(block);
        return ppdb_error_set(PPDB_BASE_ERR_MEMORY, __FILE__, __LINE__, __func__,
                             "Failed to allocate aligned memory block");
    }
    
    block->size = pool->block_size;
    block->used = 0;
    block->next = pool->head;
    pool->head = block;
    
    return PPDB_OK;
}

// Memory allocation with alignment
void* ppdb_base_aligned_alloc(size_t alignment, size_t size) {
    if (size == 0 || !IS_POWER_OF_TWO(alignment)) {
        ppdb_error_set(PPDB_BASE_ERR_PARAM, __FILE__, __LINE__, __func__,
                       "Invalid alignment or size parameters");
        return NULL;
    }
    
    size_t new_total = atomic_load(&memory_stats.current_usage) + size;
    if (memory_stats.memory_limit > 0 && new_total > memory_stats.memory_limit) {
        ppdb_error_set(PPDB_BASE_ERR_MEMORY_LIMIT, __FILE__, __LINE__, __func__,
                       "Memory limit exceeded: current=%zu, requested=%zu, limit=%zu",
                       atomic_load(&memory_stats.current_usage), size,
                       memory_stats.memory_limit);
        return NULL;
    }
    
    void* ptr = aligned_alloc(alignment, size);
    if (!ptr) {
        ppdb_error_set(PPDB_BASE_ERR_MEMORY, __FILE__, __LINE__, __func__,
                       "Failed to allocate aligned memory");
        return NULL;
    }
    
    atomic_fetch_add(&memory_stats.total_allocated, size);
    size_t current = atomic_fetch_add(&memory_stats.current_usage, size) + size;
    size_t peak = atomic_load(&memory_stats.peak_usage);
    while (current > peak) {
        if (atomic_compare_exchange_weak(&memory_stats.peak_usage, &peak, current)) {
            break;
        }
        peak = atomic_load(&memory_stats.peak_usage);
    }
    
    return ptr;
}

// Aligned memory deallocation
void ppdb_base_aligned_free(void* ptr) {
    if (!ptr) return;

    size_t* size_ptr = (size_t*)ptr - 1;
    size_t size = *size_ptr;

    #ifdef _WIN32
    _aligned_free(ptr);
    #else
    free(ptr);
    #endif

    update_memory_stats(size, false);
}

// Set memory usage limit
void ppdb_base_set_memory_limit(size_t limit) {
    memory_stats.memory_limit = limit;
}

// Get memory statistics
void ppdb_base_get_memory_stats(ppdb_base_memory_stats_t* stats) {
    if (!stats) return;
    ppdb_base_mutex_lock(memory_stats.stats_mutex);
    stats->total_allocated = memory_stats.total_allocated;
    stats->total_freed = memory_stats.total_freed;
    stats->current_usage = memory_stats.current_usage;
    stats->peak_usage = memory_stats.peak_usage;
    stats->memory_limit = memory_stats.memory_limit;
    ppdb_base_mutex_unlock(memory_stats.stats_mutex);
}

// Memory pool functions
ppdb_error_t ppdb_base_mempool_create(ppdb_base_mempool_t** pool, size_t block_size, size_t alignment) {
    if (!pool || block_size == 0 || !IS_POWER_OF_TWO(alignment)) {
        return ppdb_error_set(PPDB_BASE_ERR_PARAM, __FILE__, __LINE__, __func__,
                             "Invalid parameters: pool=%p, block_size=%zu, alignment=%zu",
                             pool, block_size, alignment);
    }

    ppdb_base_mempool_t* new_pool = (ppdb_base_mempool_t*)malloc(sizeof(ppdb_base_mempool_t));
    if (!new_pool) {
        return ppdb_error_set(PPDB_BASE_ERR_MEMORY, __FILE__, __LINE__, __func__,
                             "Failed to allocate memory pool");
    }

    new_pool->head = NULL;
    new_pool->block_size = block_size;
    new_pool->alignment = alignment;

    *pool = new_pool;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mempool_destroy(ppdb_base_mempool_t* pool) {
    if (!pool) return PPDB_OK;

    ppdb_base_mempool_block_t* block = pool->head;
    while (block) {
        ppdb_base_mempool_block_t* next = block->next;
        free(block->data);
        free(block);
        block = next;
    }

    free(pool);
    return PPDB_OK;
}

void* ppdb_base_mempool_alloc(ppdb_base_mempool_t* pool, size_t size) {
    if (!pool || size == 0 || size > pool->block_size) {
        ppdb_error_set(PPDB_BASE_ERR_PARAM, __FILE__, __LINE__, __func__,
                       "Invalid parameters: pool=%p, size=%zu", pool, size);
        return NULL;
    }

    // Find a block with enough space
    ppdb_base_mempool_block_t* block = pool->head;
    while (block) {
        size_t aligned_used = (block->used + pool->alignment - 1) & ~(pool->alignment - 1);
        if (aligned_used + size <= block->size) {
            block->used = aligned_used + size;
            return (char*)block->data + aligned_used;
        }
        block = block->next;
    }

    // Create a new block
    ppdb_error_t err = mempool_create_block(pool);
    if (err != PPDB_OK) {
        return NULL;
    }

    // Allocate from the new block
    block = pool->head;
    block->used = size;
    return block->data;
}

void ppdb_base_mempool_free(ppdb_base_mempool_t* pool, void* ptr) {
    if (!pool || !ptr) return;

    // Find the block containing this pointer
    ppdb_base_mempool_block_t* block = pool->head;
    while (block) {
        char* block_start = (char*)block->data;
        char* block_end = block_start + block->size;
        if ((char*)ptr >= block_start && (char*)ptr < block_end) {
            // Found the block, but we don't actually free individual allocations
            // They will be freed when the block or pool is destroyed
            return;
        }
        block = block->next;
    }

    // Pointer not found in any block
    ppdb_error_set(PPDB_BASE_ERR_PARAM, __FILE__, __LINE__, __func__,
                   "Invalid pointer: %p not found in pool %p", ptr, pool);
} 