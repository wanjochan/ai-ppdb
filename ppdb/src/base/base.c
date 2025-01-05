/*
 * base.c - PPDB基础设施层实现
 *
 * 包含以下模块：
 * 1. 内存管理 (base_memory.inc.c)
 * 2. 数据结构 (base_struct.inc.c)
 * 3. 同步原语 (base_sync.inc.c)
 * 4. 工具函数 (base_utils.inc.c)
 */

#include <cosmopolitan.h>
#include <internal/base.h>

// Include implementation files
#include "base_memory.inc.c"   // 内存管理实现
#include "base_struct.inc.c"   // 数据结构实现
#include "base_sync.inc.c"     // 同步原语实现
#include "base_utils.inc.c"    // 工具函数实现

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

    err = ppdb_base_sync_init(new_base);
    if (err != PPDB_OK) goto error;

    err = ppdb_base_utils_init(new_base);
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

    ppdb_base_utils_cleanup(base);
    ppdb_base_sync_cleanup(base);
    ppdb_base_memory_cleanup(base);

    ppdb_base_aligned_free(base);
} 