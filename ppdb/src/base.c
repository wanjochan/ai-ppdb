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

    // Configuration
    ppdb_base_config_t config;

    // Statistics
    struct {
        _Atomic(uint64_t) total_allocs;
        _Atomic(uint64_t) total_frees;
        _Atomic(uint64_t) current_memory;
        _Atomic(uint64_t) peak_memory;
    } stats;
};

// Include implementation files
#include "base/base_memory.inc.c"
#include "base/base_error.inc.c"
#include "base/base_sync.inc.c"
#include "base/base_struct.inc.c"
#include "base/base_utils.inc.c"
#include "base/base_skiplist.inc.c"
#include "base/base_counter.inc.c"
#include "base/base_timer.inc.c"

// Base layer initialization
ppdb_error_t ppdb_base_init(ppdb_base_t** base, const ppdb_base_config_t* config) {
    PPDB_CHECK_NULL(base);
    PPDB_CHECK_NULL(config);

    ppdb_base_t* new_base = (ppdb_base_t*)malloc(sizeof(ppdb_base_t));
    if (new_base == NULL) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_base, 0, sizeof(ppdb_base_t));
    new_base->config = *config;

    // Initialize subsystems
    PPDB_RETURN_IF_ERROR(ppdb_base_memory_init(new_base));
    PPDB_RETURN_IF_ERROR(ppdb_base_sync_init(new_base));

    *base = new_base;
    return PPDB_OK;
}

void ppdb_base_destroy(ppdb_base_t* base) {
    if (base == NULL) {
        return;
    }

    // Cleanup subsystems in reverse order
    ppdb_base_sync_cleanup(base);
    ppdb_base_memory_cleanup(base);

    free(base);
}

void ppdb_base_get_stats(ppdb_base_t* base, ppdb_base_stats_t* stats) {
    if (base == NULL || stats == NULL) {
        return;
    }

    memset(stats, 0, sizeof(ppdb_base_stats_t));
    ppdb_base_memory_get_stats(base, stats);
}
