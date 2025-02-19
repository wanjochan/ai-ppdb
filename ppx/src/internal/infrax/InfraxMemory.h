#ifndef INFRAX_MEMORY_H
#define INFRAX_MEMORY_H

//design pattern: factory

#include "InfraxCore.h"

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
    InfraxBool is_used;
    InfraxBool is_gc_root;
    uint8_t padding[6];  // 保持8字节对齐
} __attribute__((aligned(8))) MemoryBlock;

// Memory configuration
typedef struct {
    size_t initial_size;     // 初始内存大小
    InfraxBool use_gc;      // 是否使用GC
    InfraxBool use_pool;    // 是否使用内存池
    size_t gc_threshold;    // GC触发阈值
} InfraxMemoryConfig;

// Forward declarations
typedef struct InfraxMemory InfraxMemory;
typedef struct InfraxMemoryClassType InfraxMemoryClassType;

// The "static" interface (like static methods in OOP)
struct InfraxMemoryClassType {
    InfraxMemory* (*new)(const InfraxMemoryConfig* config);
    void (*free)(InfraxMemory* self);
};

// The instance structure
struct InfraxMemory {
    InfraxMemory* self;
    //InfraxMemoryClassType* klass;//InfraxMemoryClass

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

    // Instance methods (WARNING: don't move to InfraxMemoryClassType)
    void* (*alloc)(InfraxMemory* self, size_t size);
    void (*dealloc)(InfraxMemory* self, void* ptr); 
    void* (*realloc)(InfraxMemory* self, void* ptr, size_t size);
    void (*get_stats)(const InfraxMemory* self, InfraxMemoryStats* stats);
    void (*collect)(InfraxMemory* self);
};

// The "static" interface instance (like Java's Class object)
extern InfraxMemoryClassType InfraxMemoryClass;

#endif // INFRAX_MEMORY_H
