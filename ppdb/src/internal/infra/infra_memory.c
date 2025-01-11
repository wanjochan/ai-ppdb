/*
 * infra_memory.c - Memory Management Module Implementation
 */

#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Global State
//-----------------------------------------------------------------------------

static struct {
    bool initialized;
    infra_mutex_t mutex;
    infra_memory_config_t config;
    infra_memory_stats_t stats;
} g_memory = {0};

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

    // TODO: 如果启用内存池，在这里初始化

    g_memory.initialized = true;
    return INFRA_OK;
}

void infra_memory_cleanup(void) {
    if (!g_memory.initialized) {
        return;
    }

    // TODO: 如果启用内存池，在这里清理

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

    void* ptr = malloc(size);
    if (ptr) {
        infra_mutex_lock(g_memory.mutex);
        g_memory.stats.current_usage += size;
        g_memory.stats.total_allocations++;
        if (g_memory.stats.current_usage > g_memory.stats.peak_usage) {
            g_memory.stats.peak_usage = g_memory.stats.current_usage;
        }
        infra_mutex_unlock(g_memory.mutex);
    }

    return ptr;
}

void* infra_calloc(size_t nmemb, size_t size) {
    if (!g_memory.initialized) {
        return NULL;
    }

    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    void* ptr = calloc(nmemb, size);
    if (ptr) {
        infra_mutex_lock(g_memory.mutex);
        g_memory.stats.current_usage += (nmemb * size);
        g_memory.stats.total_allocations++;
        if (g_memory.stats.current_usage > g_memory.stats.peak_usage) {
            g_memory.stats.peak_usage = g_memory.stats.current_usage;
        }
        infra_mutex_unlock(g_memory.mutex);
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

    void* new_ptr = realloc(ptr, size);
    if (new_ptr) {
        // TODO: 更新内存统计信息
        // 注意：这里需要知道原始分配的大小才能准确更新统计信息
        infra_mutex_lock(g_memory.mutex);
        g_memory.stats.total_allocations++;
        infra_mutex_unlock(g_memory.mutex);
    }

    return new_ptr;
}

void infra_free(void* ptr) {
    if (!g_memory.initialized || !ptr) {
        return;
    }

    // TODO: 更新内存统计信息
    // 注意：这里需要知道释放的内存大小才能准确更新统计信息

    free(ptr);
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