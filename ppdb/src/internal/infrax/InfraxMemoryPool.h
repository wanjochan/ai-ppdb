#ifndef INFRAX_MEMORY_POOL_H
#define INFRAX_MEMORY_POOL_H

#include <stddef.h>
#include <stdbool.h>
#include "InfraxMemoryBase.h"

// Memory block structure
typedef struct MemoryBlock {
    struct MemoryBlock* next;    // 链表指针
    size_t size;                 // 块大小
    size_t original_size;        // 原始分配大小
    bool is_used;               // 使用标志
    uint8_t padding[7];         // 对齐填充，确保 8 字节对齐
} __attribute__((aligned(8))) MemoryBlock;

// 内存池实现
typedef struct InfraxMemoryPool {
    // Base memory interface
    InfraxMemoryBase base;
    
    // Pool specific members
    InfraxMemoryPoolConfig config;
    InfraxMemoryStats stats;
    void* pool_start;              // 内存池起始地址
    size_t pool_size;             // 内存池总大小
    MemoryBlock* free_list; // 空闲块链表
} InfraxMemoryPool;

// Constructor and destructor
InfraxMemoryPool* infrax_memory_pool_new(void);
void infrax_memory_pool_free(InfraxMemoryPool* self);

// Configuration
bool infrax_memory_pool_set_config(InfraxMemoryPool* self, const InfraxMemoryPoolConfig* config);

#endif // INFRAX_MEMORY_POOL_H
