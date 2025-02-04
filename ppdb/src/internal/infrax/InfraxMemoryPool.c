#include <stdlib.h>
#include <string.h>
#include "InfraxMemoryPool.h"

// 内存块结构
typedef struct MemoryBlock {
    struct MemoryBlock* next;    // 链表指针
    size_t size;                 // 块大小
    size_t original_size;        // 原始分配大小
    bool is_used;               // 使用标志
    uint8_t padding[7];         // 对齐填充，确保 8 字节对齐
} __attribute__((aligned(8))) MemoryBlock;

// 常量定义
#define MIN_BLOCK_SIZE (64)  // 最小块大小，确保足够大
#define DEFAULT_POOL_SIZE (1024 * 1024)  // 1MB
#define DEFAULT_ALIGNMENT (8)
#define ALIGN_SIZE(size, align) (((size) + ((align) - 1)) & ~((align) - 1))

// 私有函数声明
static void* pool_alloc(InfraxMemoryBase* base, size_t size);
static void* pool_realloc(InfraxMemoryBase* base, void* ptr, size_t new_size);
static void pool_dealloc(InfraxMemoryBase* base, void* ptr);
static void* pool_memset(InfraxMemoryBase* base, void* ptr, int value, size_t size);
static void pool_get_stats(InfraxMemoryBase* base, InfraxMemoryStats* stats);
static void pool_reset_stats(InfraxMemoryBase* base);

static MemoryBlock* find_free_block(InfraxMemoryPool* self, size_t size);
static MemoryBlock* split_block(MemoryBlock* block, size_t size);

// 前向声明
static bool is_block_valid(InfraxMemoryPool* self, MemoryBlock* block);
static bool is_ptr_in_pool(InfraxMemoryPool* self, void* ptr);
static void try_merge_blocks(InfraxMemoryPool* self);
static void cleanup_pool(InfraxMemoryPool* self);

static bool initialize_pool(InfraxMemoryPool* self);

// 构造函数
InfraxMemoryPool* infrax_memory_pool_new(void) {
    InfraxMemoryPool* self = (InfraxMemoryPool*)calloc(1, sizeof(InfraxMemoryPool));
    if (!self) return NULL;
    
    // 初始化基础结构
    self->base.new = (InfraxMemoryBase* (*)(void))infrax_memory_pool_new;
    self->base.free = (void (*)(InfraxMemoryBase*))infrax_memory_pool_free;
    self->base.alloc = pool_alloc;
    self->base.realloc = pool_realloc;
    self->base.dealloc = pool_dealloc;
    self->base.memset = pool_memset;
    self->base.get_stats = pool_get_stats;
    self->base.reset_stats = pool_reset_stats;
    
    // 初始化内存池配置
    self->config.initial_size = DEFAULT_POOL_SIZE;
    self->config.alignment = DEFAULT_ALIGNMENT;
    
    // 初始化内存池
    if (!initialize_pool(self)) {
        free(self);
        return NULL;
    }
    
    return self;
}

// 析构函数
void infrax_memory_pool_free(InfraxMemoryPool* self) {
    if (!self) return;
    
    // 清理内存池
    cleanup_pool(self);
    
    // 释放对象本身
    free(self);
}

// 初始化内存池
static bool initialize_pool(InfraxMemoryPool* self) {
    if (!self) return false;
    
    // 确保配置有效
    if (self->config.initial_size == 0) {
        self->config.initial_size = DEFAULT_POOL_SIZE;
    }
    if (self->config.alignment == 0) {
        self->config.alignment = DEFAULT_ALIGNMENT;
    }
    
    // 分配内存池，确保有足够空间容纳至少一个内存块
    size_t min_pool_size = sizeof(MemoryBlock) + MIN_BLOCK_SIZE;
    size_t requested_size = self->config.initial_size > min_pool_size ? 
                           self->config.initial_size : min_pool_size;
    
    // 对齐池大小
    self->pool_size = ALIGN_SIZE(requested_size, self->config.alignment);
    
    // 分配对齐的内存
    int ret = posix_memalign(&self->pool_start, self->config.alignment, self->pool_size);
    if (ret != 0) return false;
    
    // 初始化第一个块
    MemoryBlock* first_block = (MemoryBlock*)self->pool_start;
    first_block->next = NULL;
    first_block->size = self->pool_size - sizeof(MemoryBlock);
    first_block->original_size = first_block->size;
    first_block->is_used = false;
    memset(first_block->padding, 0, sizeof(first_block->padding));
    
    // 设置空闲链表
    self->free_list = first_block;
    
    // 初始化统计信息
    memset(&self->stats, 0, sizeof(InfraxMemoryPoolStats));
    
    return true;
}

// 清理内存池
static void cleanup_pool(InfraxMemoryPool* self) {
    if (!self) return;
    
    // 释放所有内存块
    if (self->pool_start) {
        // 确保所有块都标记为未使用
        MemoryBlock* current = self->free_list;
        while (current) {
            current->is_used = false;
            current = current->next;
        }
        
        // 尝试合并所有块
        try_merge_blocks(self);
        
        // 释放内存池
        free(self->pool_start);
        self->pool_start = NULL;
    }
    
    // 重置状态
    self->pool_size = 0;
    self->free_list = NULL;
    memset(&self->stats, 0, sizeof(InfraxMemoryPoolStats));
}

// 检查指针是否在内存池范围内
static bool is_ptr_in_pool(InfraxMemoryPool* self, void* ptr) {
    if (!self || !self->pool_start || !ptr) return false;
    
    // 计算内存池的范围
    uintptr_t pool_start = (uintptr_t)self->pool_start;
    uintptr_t pool_end = pool_start + self->pool_size;
    uintptr_t ptr_addr = (uintptr_t)ptr;
    
    // 检查指针是否在范围内
    return (ptr_addr >= pool_start && ptr_addr < pool_end);
}

// 查找空闲块
static MemoryBlock* find_free_block(InfraxMemoryPool* self, size_t size) {
    if (!self || !self->free_list) return NULL;
    
    // 最佳适配算法
    MemoryBlock* block = self->free_list;
    MemoryBlock* best_fit = NULL;
    size_t min_size_diff = SIZE_MAX;

    while (block) {
        if (!block->is_used && block->size >= size) {
            size_t size_diff = block->size - size;
            if (size_diff < min_size_diff) {
                min_size_diff = size_diff;
                best_fit = block;
                if (size_diff == 0) break;  // 完美匹配
            }
        }
        block = block->next;
    }

    return best_fit;
}

// 分割内存块
static MemoryBlock* split_block(MemoryBlock* block, size_t size) {
    size_t aligned_size = ALIGN_SIZE(size, DEFAULT_ALIGNMENT);
    size_t total_size = sizeof(MemoryBlock) + aligned_size;
    size_t remaining = block->size - total_size;

    if (remaining >= MIN_BLOCK_SIZE) {
        MemoryBlock* new_block = (MemoryBlock*)((char*)block + total_size);
        new_block->size = remaining - sizeof(MemoryBlock);
        new_block->is_used = false;
        new_block->next = block->next;
        
        block->size = aligned_size;
        block->next = new_block;
    }

    return block;
}

// 尝试合并相邻的空闲块
static void try_merge_blocks(InfraxMemoryPool* self) {
    if (!self || !self->free_list) return;
    
    bool merged;
    do {
        merged = false;
        MemoryBlock* current = self->free_list;
        
        while (current && current->next) {
            if (!current->is_used && !current->next->is_used) {
                // 计算下一个块的地址
                MemoryBlock* next = current->next;
                size_t total_size = sizeof(MemoryBlock) + current->size;
                if ((char*)current + total_size == (char*)next) {
                    // 合并块
                    current->size += sizeof(MemoryBlock) + next->size;
                    current->next = next->next;
                    merged = true;
                }
            }
            if (!merged) {
                current = current->next;
            }
        }
    } while (merged);
}

// 内存分配
static void* pool_alloc(InfraxMemoryBase* base, size_t size) {
    if (!base || size == 0) return NULL;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    
    // 对齐请求的大小
    size_t aligned_size = ALIGN_SIZE(size, self->config.alignment);
    
    // 查找合适的块
    MemoryBlock* best_block = NULL;
    MemoryBlock* current = self->free_list;
    size_t min_size_diff = SIZE_MAX;
    
    while (current) {
        if (!current->is_used && current->size >= aligned_size) {
            size_t size_diff = current->size - aligned_size;
            if (size_diff < min_size_diff) {
                min_size_diff = size_diff;
                best_block = current;
                if (size_diff == 0) break;  // 完美匹配
            }
        }
        current = current->next;
    }
    
    if (!best_block) return NULL;
    
    // 检查块是否有效
    if (!is_block_valid(self, best_block)) {
        return NULL;
    }
    
    // 如果块太大，需要分割
    if (min_size_diff >= sizeof(MemoryBlock) + MIN_BLOCK_SIZE) {
        size_t new_block_total_size = sizeof(MemoryBlock) + aligned_size;
        size_t remaining_size = best_block->size - new_block_total_size;
        
        // 创建新的空闲块
        MemoryBlock* new_free_block = (MemoryBlock*)((char*)best_block + new_block_total_size);
        new_free_block->next = best_block->next;
        new_free_block->size = remaining_size;
        new_free_block->original_size = remaining_size;
        new_free_block->is_used = false;
        memset(new_free_block->padding, 0, sizeof(new_free_block->padding));
        
        // 更新当前块
        best_block->next = new_free_block;
        best_block->size = aligned_size;
        best_block->original_size = aligned_size;
    }
    
    // 标记块为已使用
    best_block->is_used = true;
    
    // 更新统计信息
    self->stats.base_stats.total_allocations++;
    self->stats.base_stats.current_usage += best_block->size;
    
    // 返回可用内存区域
    return (void*)((char*)best_block + sizeof(MemoryBlock));
}

// 内存释放
static void pool_dealloc(InfraxMemoryBase* base, void* ptr) {
    if (!base || !ptr) return;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    
    // 获取块头
    MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    
    // 验证块
    if (!is_block_valid(self, block) || !block->is_used) {
        return;
    }
    
    // 标记为未使用
    block->is_used = false;
    
    // 更新统计信息
    if (self->stats.base_stats.current_usage >= block->size) {
        self->stats.base_stats.current_usage -= block->size;
    }
    
    // 尝试合并相邻的空闲块
    try_merge_blocks(self);
}

// 内存重分配
static void* pool_realloc(InfraxMemoryBase* base, void* ptr, size_t new_size) {
    if (!base) return NULL;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    
    // 如果指针为空，相当于 alloc
    if (!ptr) {
        return pool_alloc(base, new_size);
    }
    
    // 如果新大小为 0，相当于 dealloc
    if (new_size == 0) {
        pool_dealloc(base, ptr);
        return NULL;
    }
    
    // 检查指针是否在内存池范围内
    if (!is_ptr_in_pool(self, ptr)) {
        return NULL;
    }
    
    // 获取当前块
    MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    if (!block->is_used) {
        return NULL;
    }
    
    // 计算对齐后的大小
    size_t aligned_size = ALIGN_SIZE(new_size, self->config.alignment);
    
    // 如果新大小小于等于当前大小，直接返回
    if (aligned_size <= block->size) {
        block->original_size = new_size;
        return ptr;
    }
    
    // 尝试合并后续的空闲块
    size_t total_size = block->size;
    MemoryBlock* next = block->next;
    while (next && !next->is_used) {
        if ((char*)block + sizeof(MemoryBlock) + total_size == (char*)next) {
            total_size += sizeof(MemoryBlock) + next->size;
            next = next->next;
        } else {
            break;
        }
    }
    
    // 如果合并后的空间足够，直接使用
    if (total_size >= aligned_size) {
        block->size = total_size;
        block->next = next;
        if (block->size > aligned_size + sizeof(MemoryBlock) + MIN_BLOCK_SIZE) {
            size_t split_size = sizeof(MemoryBlock) + aligned_size;
            MemoryBlock* new_block = (MemoryBlock*)((char*)block + split_size);
            new_block->size = block->size - split_size;
            new_block->original_size = new_block->size;
            new_block->is_used = false;
            new_block->next = block->next;
            
            block->size = aligned_size;
            block->next = new_block;
        }
        block->original_size = new_size;
        return ptr;
    }
    
    // 分配新块并复制数据
    void* new_ptr = pool_alloc(base, new_size);
    if (!new_ptr) {
        return NULL;
    }
    
    memcpy(new_ptr, ptr, block->original_size);
    pool_dealloc(base, ptr);
    
    return new_ptr;
}

// 内存设置
static void* pool_memset(InfraxMemoryBase* base, void* ptr, int value, size_t size) {
    if (!base || !ptr || size == 0) return NULL;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    
    // 检查指针是否在内存池范围内
    if (!is_ptr_in_pool(self, ptr)) {
        return NULL;
    }
    
    // 获取块头
    MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    if (!block->is_used) {
        return NULL;
    }
    
    // 确保不会写入超过块大小
    size_t max_size = block->size;
    if (size > max_size) {
        size = max_size;
    }
    
    return memset(ptr, value, size);
}

// 获取统计信息
static void pool_get_stats(InfraxMemoryBase* base, InfraxMemoryStats* stats) {
    if (!base || !stats) return;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    *stats = self->stats.base_stats;
}

// 重置统计信息
static void pool_reset_stats(InfraxMemoryBase* base) {
    if (!base) return;
    InfraxMemoryPool* self = (InfraxMemoryPool*)base;
    memset(&self->stats, 0, sizeof(InfraxMemoryPoolStats));
}

// 检查块是否有效
static bool is_block_valid(InfraxMemoryPool* self, MemoryBlock* block) {
    if (!self || !block) return false;
    
    // 检查块是否在内存池范围内
    if (!is_ptr_in_pool(self, block)) {
        return false;
    }
    
    // 检查块大小是否合理
    if (block->size == 0 || block->size > self->pool_size) {
        return false;
    }
    
    // 检查块对齐
    if ((uintptr_t)block % self->config.alignment != 0) {
        return false;
    }
    
    return true;
}

// 设置配置
bool infrax_memory_pool_set_config(InfraxMemoryPool* self, const InfraxMemoryPoolConfig* config) {
    if (!self || !config) return false;
    
    // 如果内存池已经初始化，需要先清理
    if (self->pool_start) {
        cleanup_pool(self);
    }
    
    // 更新配置
    self->config = *config;
    
    // 重新初始化内存池
    return initialize_pool(self);
}
