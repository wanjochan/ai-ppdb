/*
 * infra_memory.c - Memory Management Module Implementation
 */

#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_sync.h"

#define INFRA_INTERNAL
#include "internal/infra/infra_memory.h"

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
    size_t total_size = sizeof(memory_block_t) + size;
    size_t remaining = block->size - total_size;

    if (remaining >= MIN_BLOCK_SIZE) {
        memory_block_t* new_block = (memory_block_t*)((char*)block + total_size);
        new_block->size = remaining - sizeof(memory_block_t);
        new_block->is_used = false;
        new_block->next = block->next;
        
        block->size = size;
        block->next = new_block;
        
        g_memory.pool.block_count++;
    }

    return block;
}

static void* allocate_from_pool(size_t size) {
    size = ALIGN_SIZE(size, g_memory.config.pool_alignment);
    
    memory_block_t* block = find_free_block(size);
    if (!block) {
        return NULL;
    }

    block = split_block(block, size);
    block->is_used = true;
    g_memory.pool.used_size += block->size;

    return (void*)((char*)block + sizeof(memory_block_t));
}

static bool initialize_pool(void) {
    size_t total_size = g_memory.config.pool_initial_size;
    void* pool_memory = malloc(total_size);
    if (!pool_memory) {
        return false;
    }

    g_memory.pool.pool_start = pool_memory;
    g_memory.pool.pool_size = total_size;
    g_memory.pool.used_size = 0;
    g_memory.pool.block_count = 1;

    // 初始化第一个块
    memory_block_t* first_block = (memory_block_t*)pool_memory;
    first_block->size = total_size - sizeof(memory_block_t);
    first_block->is_used = false;
    first_block->next = NULL;

    g_memory.pool.free_list = first_block;
    return true;
}

static void cleanup_pool(void) {
    if (g_memory.pool.pool_start) {
        free(g_memory.pool.pool_start);
        g_memory.pool.pool_start = NULL;
    }
}

static memory_block_t* get_block_header(void* ptr) {
    return (memory_block_t*)((char*)ptr - sizeof(memory_block_t));
}

static void try_merge_blocks(void) {
    memory_block_t* current = g_memory.pool.free_list;
    
    while (current && current->next) {
        if (!current->is_used && !current->next->is_used) {
            // 合并相邻的空闲块
            current->size += sizeof(memory_block_t) + current->next->size;
            current->next = current->next->next;
            g_memory.pool.block_count--;
        } else {
            current = current->next;
        }
    }
}

//-----------------------------------------------------------------------------
// Memory Module Management
//-----------------------------------------------------------------------------

infra_error_t infra_memory_init(const infra_memory_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 验证配置
    if (config->use_memory_pool &&
        (config->pool_initial_size == 0 || config->pool_alignment == 0)) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 初始化互斥锁
    infra_error_t err = infra_mutex_create(&g_memory.mutex);
    if (err != INFRA_OK) {
        return err;
    }

    // 保存配置
    g_memory.config = *config;

    // 初始化统计信息
    g_memory.stats.current_usage = 0;
    g_memory.stats.peak_usage = 0;
    g_memory.stats.total_allocations = 0;
    g_memory.stats.pool_fragmentation = 0;
    g_memory.stats.pool_utilization = 0;

    // 如果启用内存池，初始化它
    if (config->use_memory_pool) {
        if (!initialize_pool()) {
            infra_mutex_destroy(g_memory.mutex);
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

    if (g_memory.config.use_memory_pool) {
        cleanup_pool();
    }

    infra_mutex_destroy(g_memory.mutex);
    g_memory.initialized = false;
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
    
    if (g_memory.config.use_memory_pool) {
        // 计算内存池统计信息
        stats->pool_utilization = (g_memory.pool.used_size * 100) / g_memory.pool.pool_size;
        stats->pool_fragmentation = ((g_memory.pool.block_count - 1) * sizeof(memory_block_t) * 100) 
                                  / g_memory.pool.pool_size;
    }
    
    infra_mutex_unlock(g_memory.mutex);

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Memory Management Functions
//-----------------------------------------------------------------------------

void* infra_malloc(size_t size) {
    if (!g_memory.initialized) {
        return NULL;
    }

    if (size == 0) {
        return NULL;
    }

    void* ptr = NULL;
    infra_mutex_lock(g_memory.mutex);

    if (g_memory.config.use_memory_pool) {
        ptr = allocate_from_pool(size);
    }
    
    if (!ptr) {
        ptr = malloc(size);
    }

    if (ptr) {
        g_memory.stats.current_usage += size;
        g_memory.stats.total_allocations++;
        if (g_memory.stats.current_usage > g_memory.stats.peak_usage) {
            g_memory.stats.peak_usage = g_memory.stats.current_usage;
        }
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
    if (g_memory.config.use_memory_pool && ptr >= g_memory.pool.pool_start && 
        ptr < (void*)((char*)g_memory.pool.pool_start + g_memory.pool.pool_size)) {
        // 如果是内存池中的内存
        memory_block_t* block = get_block_header(ptr);
        if (block->size >= size) {
            // 如果当前块足够大，直接使用
            split_block(block, size);
            new_ptr = ptr;
        } else {
            // 否则分配新块并复制
            new_ptr = allocate_from_pool(size);
            if (new_ptr) {
                infra_memcpy(new_ptr, ptr, block->size);
                block->is_used = false;
                g_memory.pool.used_size -= block->size;
                try_merge_blocks();
            }
        }
    }

    if (!new_ptr) {
        new_ptr = realloc(ptr, size);
    }

    if (new_ptr) {
        g_memory.stats.total_allocations++;
    }

    infra_mutex_unlock(g_memory.mutex);
    return new_ptr;
}

void infra_free(void* ptr) {
    if (!g_memory.initialized || !ptr) {
        return;
    }

    infra_mutex_lock(g_memory.mutex);

    if (g_memory.config.use_memory_pool && ptr >= g_memory.pool.pool_start && 
        ptr < (void*)((char*)g_memory.pool.pool_start + g_memory.pool.pool_size)) {
        // 如果是内存池中的内存
        memory_block_t* block = get_block_header(ptr);
        block->is_used = false;
        g_memory.pool.used_size -= block->size;
        try_merge_blocks();
    } else {
        free(ptr);
    }

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