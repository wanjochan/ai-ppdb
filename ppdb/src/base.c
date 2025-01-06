/*
 * base.c - PPDB基础设施层实现
 *
 * 包含以下模块：
 * 1. 内存管理 (base_memory.inc.c)
 * 2. 数据结构 (base_struct.inc.c)
 * 3. 同步原语 (base_sync.inc.c)
 * 4. 工具函数 (base_utils.inc.c)
 * 5. 跳表实现 (base_skiplist.inc.c)
 * 6. 错误处理 (base_error.inc.c)
 * 7. 计数器实现 (base_counter.inc.c)
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Base infrastructure layer structure definition
struct ppdb_base_s {
    // Memory management
    ppdb_base_mempool_t* global_pool;
    ppdb_base_mutex_t* mem_mutex;

    // Synchronization primitives
    ppdb_base_sync_config_t sync_config;
    ppdb_base_mutex_t* global_mutex;

    // Statistics
    struct {
        _Atomic(uint64_t) total_allocs;
        _Atomic(uint64_t) total_frees;
        _Atomic(uint64_t) current_memory;
        _Atomic(uint64_t) peak_memory;
    } stats;
};

// Include implementation files
#include "base/base_error.inc.c"    // 错误处理实现
#include "base/base_memory.inc.c"   // 内存管理实现
#include "base/base_struct.inc.c"   // 数据结构实现
#include "base/base_sync.inc.c"     // 同步原语实现
#include "base/base_utils.inc.c"    // 工具函数实现
#include "base/base_skiplist.inc.c" // 跳表实现
#include "base/base_counter.inc.c"  // 计数器实现

// Base layer initialization
ppdb_error_t ppdb_base_init(ppdb_base_t** base, const ppdb_base_config_t* config) {
    ppdb_base_t* new_base;
    ppdb_error_t err;

    PPDB_CHECK_NULL(base);
    PPDB_CHECK_NULL(config);

    // Allocate base structure
    new_base = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_t));
    if (!new_base) {
        return PPDB_ERR_MEMORY;
    }

    // Initialize components
    err = ppdb_base_memory_init(new_base);
    if (err != PPDB_OK) goto error;

    // Initialize sync configuration
    new_base->sync_config.thread_safe = config->thread_safe;
    new_base->sync_config.spin_count = 1000;
    new_base->sync_config.backoff_us = 1;

    // Create global mutex
    err = ppdb_base_mutex_create(&new_base->global_mutex);
    if (err != PPDB_OK) goto error;

    *base = new_base;
    return PPDB_OK;

error:
    ppdb_base_destroy(new_base);
    return err;
}

// Base layer cleanup
void ppdb_base_destroy(ppdb_base_t* base) {
    if (!base) return;

    if (base->global_mutex) {
        ppdb_base_mutex_destroy(base->global_mutex);
        base->global_mutex = NULL;
    }

    ppdb_base_memory_cleanup(base);
    ppdb_base_aligned_free(base);
}

// Get statistics
void ppdb_base_get_stats(ppdb_base_t* base, ppdb_base_stats_t* stats) {
    if (!base || !stats) return;

    stats->total_allocs = atomic_load(&base->stats.total_allocs);
    stats->total_frees = atomic_load(&base->stats.total_frees);
    stats->current_memory = atomic_load(&base->stats.current_memory);
    stats->peak_memory = atomic_load(&base->stats.peak_memory);
}
