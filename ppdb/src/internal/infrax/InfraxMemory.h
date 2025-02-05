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

// Forward declarations
typedef struct InfraxMemory InfraxMemory;
typedef struct InfraxMemoryClass InfraxMemoryClass;

// The "static" interface (like static methods in OOP)
struct InfraxMemoryClass {
    InfraxMemory* (*new)(const InfraxMemoryConfig* config);
    void (*free)(InfraxMemory* self);
};

// The instance structure
struct InfraxMemory {
    const InfraxMemoryClass* klass;  // 指向"类"方法表
    
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
};

// The "static" interface instance (like Java's Class object)
extern const InfraxMemoryClass InfraxMemory_CLASS;

// Memory operations
void* infrax_memory_alloc(InfraxMemory* self, size_t size);
void infrax_memory_dealloc(InfraxMemory* self, void* ptr);
void* infrax_memory_realloc(InfraxMemory* self, void* ptr, size_t size);

// Utility functions
void infrax_memory_get_stats(const InfraxMemory* self, InfraxMemoryStats* stats);
void infrax_memory_collect(InfraxMemory* self);  // 触发GC

/**
notes for gc

// 为每个脚本/协程创建独立的内存管理器
InfraxMemoryConfig config = {
    .initial_size = 64 * 1024,    // 较小的初始大小
    .use_gc = true,
    .use_pool = true,             // 配合使用内存池提高性能
    .gc_threshold = 32 * 1024     // 较小的GC阈值
};

需要注意的点：
协程切换时可能需要暂停GC
考虑实现分代GC以提高性能
对于跨协程共享的对象，需要特别处理其引用计数
可以考虑在协程yield点进行增量GC
 */
#endif // INFRAX_MEMORY_H
