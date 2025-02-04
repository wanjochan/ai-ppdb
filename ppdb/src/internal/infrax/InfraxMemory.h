#ifndef INFRAX_MEMORY_H
#define INFRAX_MEMORY_H

#include <stddef.h>
#include <stdbool.h>

// Memory statistics
typedef struct {
    size_t total_allocations;
    size_t total_deallocations;
    size_t current_usage;
    size_t peak_usage;
} InfraxMemoryStats;

// Memory block header
typedef struct MemoryBlock {
    struct MemoryBlock* next;
    size_t size;
    bool is_used;
    bool is_gc_root;
    uint8_t padding[6];  // 保持8字节对齐
} __attribute__((aligned(8))) MemoryBlock;

// Memory configuration
typedef struct {
    size_t initial_size;     // 初始内存大小
    bool use_gc;            // 是否使用GC
    bool use_pool;          // 是否使用内存池
    size_t gc_threshold;    // GC触发阈值
} InfraxMemoryConfig;

// Memory manager
typedef struct InfraxMemory {
    // Configuration
    InfraxMemoryConfig config;
    InfraxMemoryStats stats;
    
    // Memory pool data
    void* pool_start;
    size_t pool_size;
    MemoryBlock* free_list;
    
    // GC data
    MemoryBlock* gc_objects;
    void* stack_bottom;
} InfraxMemory;

// Core functions
InfraxMemory* infrax_memory_new(const InfraxMemoryConfig* config);
void infrax_memory_free(InfraxMemory* self);

// Memory operations
void* infrax_memory_alloc(InfraxMemory* self, size_t size);
void infrax_memory_dealloc(InfraxMemory* self, void* ptr);
void* infrax_memory_realloc(InfraxMemory* self, void* ptr, size_t size);

// Utility functions
void infrax_memory_get_stats(const InfraxMemory* self, InfraxMemoryStats* stats);
void infrax_memory_collect(InfraxMemory* self);  // 触发GC

#endif // INFRAX_MEMORY_H
