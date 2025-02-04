#ifndef INFRAX_MEMORY_BASE_H
#define INFRAX_MEMORY_BASE_H

#include <stddef.h>
#include <stdbool.h>

// 基础内存统计信息
typedef struct {
    size_t current_usage;    // 当前内存使用量
    size_t peak_usage;       // 峰值内存使用量
    size_t total_allocations;// 总分配次数
} InfraxMemoryStats;

// 基础内存管理接口
typedef struct InfraxMemoryBase {
    // Stats
    InfraxMemoryStats stats;

    // Methods
    struct InfraxMemoryBase* (*new)(void);
    void (*free)(struct InfraxMemoryBase*);
    
    // Memory operations
    void* (*alloc)(struct InfraxMemoryBase*, size_t size);
    void* (*realloc)(struct InfraxMemoryBase*, void* ptr, size_t new_size);
    void (*dealloc)(struct InfraxMemoryBase*, void* ptr);
    void* (*memset)(struct InfraxMemoryBase*, void* ptr, int value, size_t size);
    
    // Stats operations
    void (*get_stats)(struct InfraxMemoryBase*, InfraxMemoryStats* stats);
    void (*reset_stats)(struct InfraxMemoryBase*);
} InfraxMemoryBase;

// Constructor and destructor
InfraxMemoryBase* infrax_memory_base_new(void);
void infrax_memory_base_free(InfraxMemoryBase* self);

#endif // INFRAX_MEMORY_BASE_H
