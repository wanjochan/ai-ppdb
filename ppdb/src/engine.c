/*
 * engine.c - PPDB引擎层实现
 *
 * 本文件是PPDB引擎层的主入口，负责组织和初始化所有引擎模块。
 * 包含以下模块：
 * 1. 错误处理 (engine_error.inc.c)
 * 2. 数据结构 (engine_struct.inc.c)
 * 3. 核心功能 (engine_core.inc.c)
 * 4. 事务管理 (engine_txn.inc.c)
 * 5. IO管理 (engine_io.inc.c)
 */

#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"

// 包含各模块实现
#include "engine/engine_error.inc.c"
#include "engine/engine_struct.inc.c"
#include "engine/engine_core.inc.c"
#include "engine/engine_txn.inc.c"
#include "engine/engine_io.inc.c"

// 引擎层初始化
ppdb_error_t ppdb_engine_init(ppdb_engine_t** engine, ppdb_base_t* base) {
    ppdb_engine_t* new_engine;
    ppdb_error_t err;

    if (!engine || !base) return PPDB_ERR_PARAM;

    // 分配引擎结构
    new_engine = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_engine_t));
    if (!new_engine) return PPDB_ERR_MEMORY;

    memset(new_engine, 0, sizeof(ppdb_engine_t));
    new_engine->base = base;

    // 初始化全局互斥锁
    err = ppdb_base_mutex_create(&new_engine->global_mutex);
    if (err != PPDB_OK) goto error;

    // 初始化核心功能
    err = ppdb_engine_core_init(new_engine);
    if (err != PPDB_OK) goto error;

    // 初始化事务管理
    err = ppdb_engine_txn_init(new_engine);
    if (err != PPDB_OK) goto error;

    // 启动引擎核心
    err = ppdb_engine_core_start(new_engine);
    if (err != PPDB_OK) goto error;

    *engine = new_engine;
    return PPDB_OK;

error:
    ppdb_engine_destroy(new_engine);
    return err;
}

// 引擎层清理
void ppdb_engine_destroy(ppdb_engine_t* engine) {
    if (!engine) return;

    // 停止引擎核心
    ppdb_engine_core_stop(engine);

    // 按照依赖关系反序清理
    ppdb_engine_io_cleanup(engine);
    ppdb_engine_txn_cleanup(engine);
    ppdb_engine_core_cleanup(engine);

    if (engine->global_mutex) {
        ppdb_base_mutex_destroy(engine->global_mutex);
    }

    ppdb_base_aligned_free(engine);
}

// 获取统计信息
void ppdb_engine_get_stats(ppdb_engine_t* engine, ppdb_engine_stats_t* stats) {
    if (!engine || !stats) return;

    // Copy statistics
    memcpy(stats, &engine->stats, sizeof(ppdb_engine_stats_t));
}
