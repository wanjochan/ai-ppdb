/*
 * base.c - PPDB基础设施层实现
 *
 * 本文件是PPDB基础设施层的主入口，负责组织和初始化所有基础模块。
 * 包含以下模块：
 * 1. 内存管理 (base_memory.inc.c)
 * 2. 同步原语 (base_sync.inc.c)
 * 3. 数据结构 (base_struct.inc.c)
 * 4. 工具函数 (base_utils.inc.c)
 */

#include <cosmopolitan.h>
#include <ppdb/internal/base.h>

// 基础设施层结构定义
struct ppdb_base_s {
    // 内存管理
    ppdb_base_mempool_t* global_pool;
    ppdb_base_mutex_t* mem_mutex;

    // 同步原语
    ppdb_base_sync_config_t sync_config;
    ppdb_base_mutex_t* global_mutex;

    // 统计信息
    struct {
        atomic_uint64_t total_allocs;
        atomic_uint64_t total_frees;
        atomic_uint64_t current_memory;
        atomic_uint64_t peak_memory;
    } stats;
};

// 包含各模块实现
#include "base/base_memory.inc.c"
#include "base/base_sync.inc.c"
#include "base/base_struct.inc.c"
#include "base/base_utils.inc.c"

// 基础设施层初始化
ppdb_error_t ppdb_base_init(ppdb_base_t** base, const ppdb_base_config_t* config) {
    ppdb_base_t* new_base;
    ppdb_error_t err;

    if (!base || !config) return PPDB_ERR_PARAM;

    // 分配基础结构
    new_base = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_t));
    if (!new_base) return PPDB_ERR_MEMORY;

    memset(new_base, 0, sizeof(ppdb_base_t));

    // 初始化内存管理
    err = ppdb_base_memory_init(new_base);
    if (err != PPDB_OK) goto error;

    // 初始化同步原语
    err = ppdb_base_sync_init(new_base);
    if (err != PPDB_OK) goto error;

    // 初始化工具函数
    err = ppdb_base_utils_init(new_base);
    if (err != PPDB_OK) goto error;

    *base = new_base;
    return PPDB_OK;

error:
    ppdb_base_destroy(new_base);
    return err;
}

// 基础设施层清理
void ppdb_base_destroy(ppdb_base_t* base) {
    if (!base) return;

    // 按照依赖关系反序清理
    ppdb_base_utils_cleanup(base);
    ppdb_base_sync_cleanup(base);
    ppdb_base_memory_cleanup(base);

    ppdb_base_aligned_free(base);
}

// 获取统计信息
void ppdb_base_get_stats(ppdb_base_t* base, ppdb_base_stats_t* stats) {
    if (!base || !stats) return;

    stats->total_allocs = atomic_load(&base->stats.total_allocs);
    stats->total_frees = atomic_load(&base->stats.total_frees);
    stats->current_memory = atomic_load(&base->stats.current_memory);
    stats->peak_memory = atomic_load(&base->stats.peak_memory);
}
