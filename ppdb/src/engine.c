/*
 * engine.c - PPDB引擎层实现
 *
 * 本文件是PPDB引擎层的主入口，负责组织和初始化所有引擎模块。
 */

#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"

// Include implementation files
#include "engine/engine_txn.inc.c"
#include "engine/engine_table.inc.c"
#include "engine/engine_table_list.inc.c"
#include "engine/engine_cursor.inc.c"
#include "engine/engine_io.inc.c"
#include "engine/engine_stats.inc.c"
#include "engine/engine_mutex.inc.c"
#include "engine/engine_async.inc.c"

// Engine initialization
ppdb_error_t ppdb_engine_init(ppdb_engine_t** engine, ppdb_base_t* base) {
    if (!engine || !base) return PPDB_ENGINE_ERR_PARAM;
    if (*engine) return PPDB_ENGINE_ERR_PARAM;

    // Allocate engine structure
    ppdb_engine_t* e = malloc(sizeof(ppdb_engine_t));
    if (!e) return PPDB_ENGINE_ERR_INIT;

    // Initialize engine structure
    memset(e, 0, sizeof(ppdb_engine_t));
    e->base = base;

    // Initialize global mutex
    ppdb_error_t err = ppdb_base_mutex_create(&e->global_mutex);
    if (err != PPDB_OK) {
        free(e);
        return err;
    }

    // Initialize transaction manager
    err = ppdb_engine_txn_init(e);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(e->global_mutex);
        free(e);
        return err;
    }

    // Initialize table list
    err = ppdb_engine_table_list_create(e, &e->tables);
    if (err != PPDB_OK) {
        ppdb_engine_txn_cleanup(e);
        ppdb_base_mutex_destroy(e->global_mutex);
        free(e);
        return err;
    }

    // Initialize IO manager
    err = ppdb_engine_io_init(e);
    if (err != PPDB_OK) {
        ppdb_engine_txn_cleanup(e);
        ppdb_base_mutex_destroy(e->global_mutex);
        free(e);
        return err;
    }

    // Initialize statistics
    memset(&e->stats, 0, sizeof(ppdb_engine_stats_t));
    err = ppdb_engine_stats_init(&e->stats);
    if (err != PPDB_OK) {
        ppdb_engine_io_cleanup(e);
        ppdb_engine_txn_cleanup(e);
        ppdb_base_mutex_destroy(e->global_mutex);
        free(e);
        return err;
    }

    *engine = e;
    return PPDB_OK;
}

void ppdb_engine_destroy(ppdb_engine_t* engine) {
    if (!engine) return;

    // Cleanup statistics
    ppdb_engine_stats_cleanup(&engine->stats);

    // Cleanup IO manager
    ppdb_engine_io_cleanup(engine);

    // Cleanup transaction manager
    ppdb_engine_txn_cleanup(engine);

    // Destroy global mutex
    if (engine->global_mutex) {
        ppdb_base_mutex_destroy(engine->global_mutex);
    }

    // Free engine structure
    free(engine);
}

const char* ppdb_engine_strerror(ppdb_error_t err) {
    switch (err) {
        case PPDB_ENGINE_ERR_INIT:
            return "Engine initialization failed";
        case PPDB_ENGINE_ERR_PARAM:
            return "Invalid parameter";
        case PPDB_ENGINE_ERR_MUTEX:
            return "Mutex operation failed";
        case PPDB_ENGINE_ERR_TXN:
            return "Transaction operation failed";
        case PPDB_ENGINE_ERR_MVCC:
            return "MVCC operation failed";
        case PPDB_ENGINE_ERR_ASYNC:
            return "Async operation failed";
        case PPDB_ENGINE_ERR_TIMEOUT:
            return "Operation timed out";
        case PPDB_ENGINE_ERR_BUSY:
            return "Resource is busy";
        case PPDB_ENGINE_ERR_FULL:
            return "Resource is full";
        case PPDB_ENGINE_ERR_NOT_FOUND:
            return "Resource not found";
        case PPDB_ENGINE_ERR_EXISTS:
            return "Resource already exists";
        case PPDB_ENGINE_ERR_INVALID_STATE:
            return "Invalid state";
        case PPDB_ENGINE_ERR_MEMORY:
            return "Memory allocation failed";
        case PPDB_ENGINE_ERR_BUFFER_FULL:
            return "Buffer is too small";
        default:
            return "Unknown engine error";
    }
}
