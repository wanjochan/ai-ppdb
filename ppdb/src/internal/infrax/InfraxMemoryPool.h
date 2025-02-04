#ifndef INFRAX_MEMORY_POOL_H
#define INFRAX_MEMORY_POOL_H

#include "InfraxMemoryBase.h"

// 内存池配置
typedef struct {
    size_t initial_size;     // 内存池初始大小
    size_t alignment;        // 内存对齐大小
} InfraxMemoryPoolConfig;

// 内存池统计信息
typedef struct {
    InfraxMemoryStats base_stats;  // 基础统计信息
    size_t fragmentation;          // 内存池碎片率
    size_t utilization;           // 内存池利用率
} InfraxMemoryPoolStats;

// 内存池管理接口
typedef struct InfraxMemoryPool {
    // Base memory interface
    InfraxMemoryBase base;
    
    // Pool specific members
    InfraxMemoryPoolConfig config;
    InfraxMemoryPoolStats stats;
    void* pool_start;              // 内存池起始地址
    size_t pool_size;             // 内存池总大小
    struct MemoryBlock* free_list; // 空闲块链表

    // Pool specific methods
    void (*set_config)(struct InfraxMemoryPool*, const InfraxMemoryPoolConfig* config);
    void (*get_pool_stats)(struct InfraxMemoryPool*, InfraxMemoryPoolStats* stats);
    void (*defrag)(struct InfraxMemoryPool*);  // 碎片整理
} InfraxMemoryPool;

// Constructor and destructor
InfraxMemoryPool* infrax_memory_pool_new(void);
void infrax_memory_pool_free(InfraxMemoryPool* self);

#endif // INFRAX_MEMORY_POOL_H
