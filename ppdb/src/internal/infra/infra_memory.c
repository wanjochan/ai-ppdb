/*
 * infra_memory.c - Memory Management Module Implementation
 * support system mode and pool mode
 */

#define INFRA_INTERNAL
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MIN_BLOCK_SIZE (sizeof(memory_block_t))
#define ALIGN_SIZE(size, align) (((size) + ((align) - 1)) & ~((align) - 1))

//-----------------------------------------------------------------------------
// Global State
//-----------------------------------------------------------------------------

static struct {
    bool initialized;
    infra_mutex_t mutex;
    infra_memory_config_t config;
    infra_memory_stats_t stats;
    memory_pool_t pool;
} g_memory = {0};

//-----------------------------------------------------------------------------
// Memory Pool Functions
//-----------------------------------------------------------------------------

static memory_block_t* find_free_block(size_t size) {
    memory_block_t* block = g_memory.pool.free_list;
    memory_block_t* best_fit = NULL;
    size_t min_size_diff = SIZE_MAX;

    // 最佳适配算法
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

static memory_block_t* split_block(memory_block_t* block, size_t size) {
    size_t aligned_size = ALIGN_SIZE(size, g_memory.config.pool_alignment);
    size_t total_size = sizeof(memory_block_t) + aligned_size;
    size_t remaining = block->size - total_size;

    if (remaining >= MIN_BLOCK_SIZE) {
        memory_block_t* new_block = (memory_block_t*)((char*)block + total_size);
        new_block->size = remaining - sizeof(memory_block_t);
        new_block->is_used = false;
        new_block->next = block->next;
        
        block->size = aligned_size;
        block->next = new_block;
        
        g_memory.pool.block_count++;
    }

    return block;
}

static void* allocate_from_pool(size_t size) {
    // 首先检查 size 为 0 的情况
    if (size == 0) {
        return NULL;
    }

    // 计算需要的总大小，包括块头
    size_t total_size = sizeof(memory_block_t) + size;
    if (total_size < size) {  // 检查溢出
        return NULL;
    }

    // 计算对齐后的大小
    size_t aligned_size = ALIGN_SIZE(total_size, g_memory.config.pool_alignment);
    if (aligned_size < total_size) {  // 检查溢出
        return NULL;
    }

    // 检查是否超过池大小
    if (aligned_size > g_memory.pool.pool_size) {
        return NULL;
    }

    memory_block_t* current = g_memory.pool.free_list;
    while (current) {
        if (!current->is_used && current->size >= aligned_size) {  // 使用对齐后的大小进行比较
            // 找到足够大的空闲块
            if (current->size >= aligned_size + MIN_BLOCK_SIZE) {
                // 分割块
                memory_block_t* new_block = (memory_block_t*)((char*)current + aligned_size);
                new_block->size = current->size - aligned_size;
                new_block->is_used = false;
                new_block->next = current->next;
                current->size = aligned_size - sizeof(memory_block_t);  // 存储可用大小
                current->next = new_block;
                g_memory.pool.block_count++;
            }
            current->is_used = true;
            g_memory.pool.used_size += aligned_size;  // 使用对齐后的大小
            g_memory.stats.current_usage += size;  // 使用原始大小
            g_memory.stats.total_allocations++;
            if (g_memory.stats.current_usage > g_memory.stats.peak_usage) {
                g_memory.stats.peak_usage = g_memory.stats.current_usage;
            }
            
            // 确保返回的地址正确对齐
            void* ptr = (void*)(current + 1);
            size_t addr = (size_t)ptr;
            if ((addr & (g_memory.config.pool_alignment - 1)) != 0) {
                // 如果地址没有正确对齐，进行调整
                size_t padding = g_memory.config.pool_alignment - (addr & (g_memory.config.pool_alignment - 1));
                ptr = (void*)((char*)ptr + padding);
            }
            return ptr;
        }
        current = current->next;
    }

    return NULL;
}

static bool initialize_pool(void) {
    // 分配内存池
    g_memory.pool.pool_start = malloc(g_memory.config.pool_initial_size);
    if (!g_memory.pool.pool_start) {
        return false;
    }

    // 初始化第一个块
    memory_block_t* first_block = (memory_block_t*)g_memory.pool.pool_start;
    first_block->size = g_memory.config.pool_initial_size - sizeof(memory_block_t);
    first_block->is_used = false;
    first_block->next = NULL;

    // 初始化内存池状态
    g_memory.pool.pool_size = g_memory.config.pool_initial_size;
    g_memory.pool.used_size = 0;
    g_memory.pool.block_count = 1;  // 初始有一个块
    g_memory.pool.free_list = first_block;

    // 初始化统计信息
    memset(&g_memory.stats, 0, sizeof(g_memory.stats));

    return true;
}

static void cleanup_pool(void) {
    if (g_memory.pool.pool_start) {
        free(g_memory.pool.pool_start);
        g_memory.pool.pool_start = NULL;
    }
    
    // 完全重置所有状态
    memset(&g_memory.pool, 0, sizeof(g_memory.pool));
    memset(&g_memory.stats, 0, sizeof(g_memory.stats));
}

static memory_block_t* get_block_header(void* ptr) {
    return (memory_block_t*)((char*)ptr - sizeof(memory_block_t));
}

static void try_merge_blocks(void) {
    bool merged;
    do {
        merged = false;
        memory_block_t* current = g_memory.pool.free_list;
        while (current && current->next) {
            if (!current->is_used && !current->next->is_used) {
                // 合并相邻的空闲块
                current->size += current->next->size + sizeof(memory_block_t);
                current->next = current->next->next;
                g_memory.pool.block_count--;  // 更新块计数
                merged = true;
            } else {
                current = current->next;
            }
        }
    } while (merged);  // 继续合并直到没有可合并的块
}

//-----------------------------------------------------------------------------
// Memory Module Management
//-----------------------------------------------------------------------------

static void reset_memory_state(void) {
    bool was_initialized = g_memory.initialized;
    infra_mutex_t old_mutex = g_memory.mutex;
    memset(&g_memory, 0, sizeof(g_memory));
    if (was_initialized) {
        g_memory.mutex = old_mutex;
    }
}

infra_error_t infra_memory_init(const infra_memory_config_t* config) {
    // 检查参数
    if (!config) {
        reset_memory_state();
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 检查重复初始化
    if (g_memory.initialized) {
        return INFRA_ERROR_EXISTS;
    }

    // 验证内存池参数
    if (config->use_memory_pool) {
        if (config->pool_initial_size == 0 || 
            config->pool_alignment == 0 || 
            config->pool_alignment < sizeof(void*) || 
            (config->pool_alignment & (config->pool_alignment - 1)) != 0 ||
            config->pool_initial_size < MIN_BLOCK_SIZE) {
            reset_memory_state();
            return INFRA_ERROR_INVALID_PARAM;
        }
    }

    // 重置状态
    reset_memory_state();

    // 初始化互斥锁
    infra_error_t err = infra_mutex_create(&g_memory.mutex);
    if (err != INFRA_OK) {
        reset_memory_state();
        return err;
    }

    // 保存配置
    g_memory.config = *config;

    // 如果启用内存池，初始化它
    if (config->use_memory_pool) {
        if (!initialize_pool()) {
            infra_mutex_destroy(g_memory.mutex);
            reset_memory_state();
            return INFRA_ERROR_NO_MEMORY;
        }
    }

    g_memory.initialized = true;
    return INFRA_OK;
}

void infra_memory_cleanup(void) {
    if (!g_memory.initialized) {
        return;
    }

    infra_mutex_t mutex = g_memory.mutex;  // 保存互斥锁
    infra_mutex_lock(mutex);

    if (g_memory.config.use_memory_pool) {
        cleanup_pool();
    }

    // 重置状态（除了互斥锁）
    bool was_initialized = g_memory.initialized;
    memset(&g_memory, 0, sizeof(g_memory));
    g_memory.mutex = mutex;  // 恢复互斥锁

    infra_mutex_unlock(mutex);  // 解锁

    if (was_initialized) {
        infra_mutex_destroy(mutex);  // 最后销毁互斥锁
    }
}

infra_error_t infra_memory_get_stats(infra_memory_stats_t* stats) {
    if (!stats) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (!g_memory.initialized) {
        return INFRA_ERROR_NOT_READY;
    }

    infra_mutex_lock(g_memory.mutex);
    
    *stats = g_memory.stats;
    
    if (g_memory.config.use_memory_pool && g_memory.pool.pool_size > 0) {
        // 计算内存池统计信息
        stats->pool_utilization = (g_memory.pool.used_size * 100) / g_memory.pool.pool_size;
        
        size_t total_overhead = g_memory.pool.block_count * sizeof(memory_block_t);
        stats->pool_fragmentation = (total_overhead * 100) / g_memory.pool.pool_size;
    } else {
        stats->pool_utilization = 0;
        stats->pool_fragmentation = 0;
    }
    
    infra_mutex_unlock(g_memory.mutex);

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Memory Management Functions
//-----------------------------------------------------------------------------

static void* allocate_memory(size_t size) {
    if (size == 0) {
        size = 1;  // 确保至少分配1字节
    }

    void* ptr = NULL;
    
    if (g_memory.config.use_memory_pool) {
        // 计算对齐后的大小，确保包含额外的对齐空间
        size_t aligned_size = ALIGN_SIZE(size + g_memory.config.pool_alignment - 1, g_memory.config.pool_alignment);
        
        // 查找合适的空闲块
        memory_block_t* block = find_free_block(aligned_size);
        if (block) {
            // 如果块太大，进行分割
            if (block->size > aligned_size + sizeof(memory_block_t) + MIN_BLOCK_SIZE) {
                // 确保新块的起始地址也是对齐的
                char* new_block_addr = (char*)(block + 1) + aligned_size;
                new_block_addr = (char*)ALIGN_SIZE((uintptr_t)new_block_addr, g_memory.config.pool_alignment);
                
                memory_block_t* new_block = (memory_block_t*)new_block_addr;
                new_block->size = block->size - aligned_size - sizeof(memory_block_t);
                new_block->is_used = false;
                new_block->next = block->next;
                block->next = new_block;
                block->size = aligned_size;  // 存储对齐后的大小
                block->original_size = size;  // 存储原始大小
                g_memory.pool.block_count++;
            } else {
                block->original_size = size;  // 存储原始大小
            }
            block->is_used = true;
            g_memory.pool.used_size += aligned_size;
            g_memory.stats.current_usage += size;  // 使用原始大小
            
            // 确保返回的指针是对齐的
            char* aligned_ptr = (char*)(block + 1);
            aligned_ptr = (char*)ALIGN_SIZE((uintptr_t)aligned_ptr, g_memory.config.pool_alignment);
            ptr = aligned_ptr;
        }
    } else {
        // 系统内存分配
        size_t total_size = size + sizeof(size_t) + g_memory.config.pool_alignment - 1;
        if (total_size > size) {  // 检查溢出
            size_t* block = (size_t*)malloc(total_size);
            if (block) {
                // 确保数据部分是对齐的
                void* aligned_ptr = (void*)ALIGN_SIZE((uintptr_t)(block + 1), g_memory.config.pool_alignment);
                block = (size_t*)aligned_ptr - 1;
                *block = size;
                ptr = aligned_ptr;
                g_memory.stats.current_usage += size;
            }
        }
    }

    if (ptr && g_memory.stats.current_usage > g_memory.stats.peak_usage) {
        g_memory.stats.peak_usage = g_memory.stats.current_usage;
    }

    return ptr;
}

static void free_memory(void* ptr) {
    if (!ptr) {
        return;
    }

    if (g_memory.config.use_memory_pool) {
        memory_block_t* block = get_block_header(ptr);
        if (block && block->is_used && 
            (char*)block >= (char*)g_memory.pool.pool_start && 
            (char*)block < (char*)g_memory.pool.pool_start + g_memory.pool.pool_size) {
            size_t aligned_size = block->size;  // 已经是对齐后的大小
            size_t original_size = block->original_size;  // 获取原始大小
            block->is_used = false;
            g_memory.pool.used_size -= aligned_size;
            g_memory.stats.current_usage -= original_size;  // 使用原始大小
            try_merge_blocks();
        }
    } else {
        size_t* block = (size_t*)ptr - 1;
        size_t size = *block;
        g_memory.stats.current_usage -= size;
        free(block);
    }
}

void* infra_malloc(size_t size)
{
    if (!g_memory.initialized) {
        return NULL;
    }

    infra_mutex_lock(g_memory.mutex);
    void* ptr = allocate_memory(size);
    if (ptr) {
        g_memory.stats.total_allocations++;
    }
    infra_mutex_unlock(g_memory.mutex);
    return ptr;
}

void* infra_calloc(size_t nmemb, size_t size) {
    if (!g_memory.initialized) {
        return NULL;
    }

    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    size_t total_size = nmemb * size;
    void* ptr = infra_malloc(total_size);
    if (ptr) {
        infra_memset(ptr, 0, total_size);
    }

    return ptr;
}

void* infra_realloc(void* ptr, size_t size) {
    if (!g_memory.initialized) {
        return NULL;
    }

    if (size == 0) {
        if (ptr) {
            infra_free(ptr);
        }
        return NULL;
    }

    if (!ptr) {
        return infra_malloc(size);
    }

    infra_mutex_lock(g_memory.mutex);

    void* new_ptr = NULL;
    if (g_memory.config.use_memory_pool && 
        ptr >= g_memory.pool.pool_start && 
        ptr < (void*)((char*)g_memory.pool.pool_start + g_memory.pool.pool_size)) {
        // 如果是内存池中的内存
        memory_block_t* block = get_block_header(ptr);
        if (block->is_used) {
            size_t old_size = block->size;
            size_t aligned_old_size = ALIGN_SIZE(old_size, g_memory.config.pool_alignment);
            size_t aligned_new_size = ALIGN_SIZE(size, g_memory.config.pool_alignment);
            
            if (aligned_new_size <= block->size) {
                // 如果当前块足够大，直接使用
                block = split_block(block, aligned_new_size);
                new_ptr = ptr;
                // 更新统计信息
                g_memory.pool.used_size = g_memory.pool.used_size - aligned_old_size + aligned_new_size;
                g_memory.stats.current_usage = g_memory.stats.current_usage - aligned_old_size + aligned_new_size;
            } else {
                // 否则分配新块并复制
                new_ptr = allocate_from_pool(size);
                if (new_ptr) {
                    infra_memcpy(new_ptr, ptr, old_size);
                    // 释放旧块
                    block->is_used = false;
                    g_memory.pool.used_size -= aligned_old_size;
                    g_memory.stats.current_usage -= aligned_old_size;
                    try_merge_blocks();
                }
            }
        }
    } else if (ptr > (void*)sizeof(size_t)) {
        // 如果是系统分配的内存
        size_t* size_ptr = (size_t*)ptr - 1;
        if (size_ptr && *size_ptr > 0 && *size_ptr <= SIZE_MAX - sizeof(size_t)) {
            size_t old_size = *size_ptr;
            size_t total_size = sizeof(size_t) + size;
            
            if (total_size > size) {  // 检查溢出
                size_t* new_block = (size_t*)realloc(size_ptr, total_size);
                if (new_block) {
                    *new_block = size;  // 更新大小信息
                    new_ptr = new_block + 1;
                    // 更新统计信息
                    g_memory.stats.current_usage = g_memory.stats.current_usage - old_size + size;
                    g_memory.stats.total_allocations++;
                    if (g_memory.stats.current_usage > g_memory.stats.peak_usage) {
                        g_memory.stats.peak_usage = g_memory.stats.current_usage;
                    }
                }
            }
        }
    }

    infra_mutex_unlock(g_memory.mutex);
    return new_ptr;
}

void infra_free(void* ptr) {
    if (!g_memory.initialized || !ptr) {
        return;
    }

    infra_mutex_lock(g_memory.mutex);
    free_memory(ptr);
    infra_mutex_unlock(g_memory.mutex);
}

//-----------------------------------------------------------------------------
// Memory Operations
//-----------------------------------------------------------------------------

void* infra_memset(void* s, int c, size_t n) {
    if (!s || n == 0) {
        return s;
    }
    return memset(s, c, n);
}

void* infra_memcpy(void* dest, const void* src, size_t n) {
    if (!dest || !src || n == 0) {
        return dest;
    }
    return memcpy(dest, src, n);
}

void* infra_memmove(void* dest, const void* src, size_t n) {
    if (!dest || !src || n == 0) {
        return dest;
    }
    return memmove(dest, src, n);
}

int infra_memcmp(const void* s1, const void* s2, size_t n) {
    if (!s1 || !s2) {
        return s1 ? 1 : (s2 ? -1 : 0);
    }
    if (n == 0) {
        return 0;
    }
    return memcmp(s1, s2, n);
}

void* infra_mem_map(void *addr, size_t size, int prot)
{
    if (size == 0) {
        return NULL;
    }

    void *mem = mmap(addr, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        return NULL;
    }
    return mem;
}

infra_error_t infra_mem_unmap(void *addr, size_t size)
{
    if (munmap(addr, size) != 0) {
        return INFRA_ERROR_NO_MEMORY;
    }
    return INFRA_OK;
}

infra_error_t infra_mem_protect(void* addr, size_t size, int prot)
{
    if (!addr || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (mprotect(addr, size, prot) != 0) {
        return INFRA_ERROR_NO_MEMORY;
    }

    return INFRA_OK;
} 
