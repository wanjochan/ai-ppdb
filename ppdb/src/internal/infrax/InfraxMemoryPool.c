#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "InfraxMemoryBase.h"
#include "InfraxMemoryPool.h"

// Default values and constants
#define DEFAULT_POOL_SIZE (1024 * 1024)  // 1MB
#define DEFAULT_ALIGNMENT 8
#define MIN_BLOCK_SIZE 32
#define MAX_POOL_SIZE (1024 * 1024 * 1024)  // 1GB
#define MIN_ALIGNMENT 4

// Alignment macro
#define ALIGN_SIZE(size, align) (((size) + ((align) - 1)) & ~((align) - 1))

// Forward declarations
static void* pool_alloc(InfraxMemoryBase* base, size_t size);
static void* pool_realloc(InfraxMemoryBase* base, void* ptr, size_t new_size);
static void pool_dealloc(InfraxMemoryBase* base, void* ptr);
static void* pool_memset(InfraxMemoryBase* base, void* ptr, int value, size_t size);
static void pool_get_stats(InfraxMemoryBase* base, InfraxMemoryStats* stats);
static void pool_reset_stats(InfraxMemoryBase* base);
static void pool_set_config(InfraxMemoryBase* base, const InfraxMemoryConfig* config);

// Memory block validation
static bool is_block_valid(InfraxMemoryPool* self, MemoryBlock* block) {
    if (!self || !block) return false;
    char* pool_end = (char*)self->pool_start + self->pool_size;
    return (char*)block >= (char*)self->pool_start && 
           (char*)block + sizeof(MemoryBlock) <= pool_end;
}

// Public configuration function
bool infrax_memory_pool_set_config(InfraxMemoryPool* self, const InfraxMemoryPoolConfig* config) {
    if (!self || !config) return false;
    
    // Copy configuration
    memcpy(&self->config, config, sizeof(InfraxMemoryPoolConfig));
    
    // Ensure valid configuration
    if (self->config.initial_size == 0) {
        self->config.initial_size = DEFAULT_POOL_SIZE;
    }
    if (self->config.alignment == 0) {
        self->config.alignment = DEFAULT_ALIGNMENT;
    }
    
    // Align initial size
    self->config.initial_size = ALIGN_SIZE(self->config.initial_size, self->config.alignment);
    
    // Free old pool if exists
    if (self->pool_start) {
        free(self->pool_start);
        self->pool_start = NULL;
        self->pool_size = 0;
        self->free_list = NULL;
    }
    
    // Allocate pool with extra space for alignment
    size_t aligned_size = self->config.initial_size + self->config.alignment;
    self->pool_start = malloc(aligned_size);
    if (!self->pool_start) return false;
    
    // Align pool start address
    uintptr_t addr = (uintptr_t)self->pool_start;
    uintptr_t aligned_addr = ALIGN_SIZE(addr, self->config.alignment);
    self->pool_start = (void*)aligned_addr;
    self->pool_size = self->config.initial_size;
    
    // Initialize first block
    MemoryBlock* first_block = (MemoryBlock*)self->pool_start;
    first_block->size = self->pool_size - sizeof(MemoryBlock);
    first_block->original_size = first_block->size;
    first_block->is_used = false;
    first_block->next = NULL;
    
    // Set free list
    self->free_list = first_block;
    
    // Reset statistics
    memset(&self->stats, 0, sizeof(InfraxMemoryStats));
    
    return true;
}

// Internal configuration function
static void pool_set_config(InfraxMemoryBase* base, const InfraxMemoryConfig* config) {
    if (!base || !config) return;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    infrax_memory_pool_set_config(self, &config->pool_config);
}

// Constructor
InfraxMemoryPool* infrax_memory_pool_new(void) {
    InfraxMemoryPool* self = (InfraxMemoryPool*)malloc(sizeof(InfraxMemoryPool));
    if (!self) return NULL;
    
    // Initialize base memory interface
    self->base.alloc = pool_alloc;
    self->base.realloc = pool_realloc;
    self->base.dealloc = pool_dealloc;
    self->base.memset = pool_memset;
    self->base.get_stats = pool_get_stats;
    self->base.reset_stats = pool_reset_stats;
    
    // Initialize pool members
    self->pool_start = NULL;
    self->pool_size = 0;
    self->free_list = NULL;
    memset(&self->stats, 0, sizeof(InfraxMemoryStats));
    
    return self;
}

// Destructor
void infrax_memory_pool_free(InfraxMemoryPool* self) {
    if (!self) return;
    if (self->pool_start) {
        free(self->pool_start);
    }
    free(self);
}

// Memory operations
static void* pool_alloc(InfraxMemoryBase* base, size_t size) {
    if (!base || size == 0) return NULL;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    
    // Align size
    size = ALIGN_SIZE(size, self->config.alignment);
    
    // Find best fit block
    MemoryBlock* best_block = NULL;
    MemoryBlock* current = self->free_list;
    size_t min_size = SIZE_MAX;
    
    while (current) {
        if (!current->is_used && current->size >= size && current->size < min_size) {
            best_block = current;
            min_size = current->size;
        }
        current = current->next;
    }
    
    if (!best_block) return NULL;
    
    // Split block if possible
    if (best_block->size >= size + sizeof(MemoryBlock) + MIN_BLOCK_SIZE) {
        MemoryBlock* new_block = (MemoryBlock*)((char*)best_block + sizeof(MemoryBlock) + size);
        new_block->size = best_block->size - size - sizeof(MemoryBlock);
        new_block->original_size = new_block->size;
        new_block->is_used = false;
        new_block->next = best_block->next;
        
        best_block->size = size;
        best_block->next = new_block;
    }
    
    best_block->is_used = true;
    
    // Update statistics
    self->stats.total_allocations++;
    self->stats.current_usage += best_block->size;
    if (self->stats.current_usage > self->stats.peak_usage) {
        self->stats.peak_usage = self->stats.current_usage;
    }
    
    return (char*)best_block + sizeof(MemoryBlock);
}

static void pool_dealloc(InfraxMemoryBase* base, void* ptr) {
    if (!base || !ptr) return;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    
    MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    if (!is_block_valid(self, block)) return;
    
    block->is_used = false;
    
    // Update statistics
    self->stats.total_deallocations++;
    if (self->stats.current_usage >= block->size) {
        self->stats.current_usage -= block->size;
    }
    
    // Try to merge adjacent free blocks
    MemoryBlock* current = self->free_list;
    while (current) {
        if (!current->is_used && current->next && !current->next->is_used) {
            current->size += sizeof(MemoryBlock) + current->next->size;
            current->next = current->next->next;
            continue;
        }
        current = current->next;
    }
}

static void* pool_realloc(InfraxMemoryBase* base, void* ptr, size_t new_size) {
    if (!base) return NULL;
    if (!ptr) return pool_alloc(base, new_size);
    if (new_size == 0) {
        pool_dealloc(base, ptr);
        return NULL;
    }
    
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    if (!is_block_valid(self, block)) return NULL;
    
    // If new size fits in current block, just return the same pointer
    if (new_size <= block->size) return ptr;
    
    // Allocate new block
    void* new_ptr = pool_alloc(base, new_size);
    if (!new_ptr) return NULL;
    
    // Copy data and free old block
    memcpy(new_ptr, ptr, block->size);
    pool_dealloc(base, ptr);
    
    return new_ptr;
}

static void* pool_memset(InfraxMemoryBase* base, void* ptr, int value, size_t size) {
    if (!base || !ptr) return NULL;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    
    MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    if (!is_block_valid(self, block)) return NULL;
    
    return memset(ptr, value, size);
}

static void pool_get_stats(InfraxMemoryBase* base, InfraxMemoryStats* stats) {
    if (!base || !stats) return;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    *stats = self->stats;
}

static void pool_reset_stats(InfraxMemoryBase* base) {
    if (!base) return;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    memset(&self->stats, 0, sizeof(InfraxMemoryStats));
}
