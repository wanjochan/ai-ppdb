/*
 * engine_core.inc.c - Engine core functionality implementation
 */

#include <cosmopolitan.h>
#include "internal/engine.h"

// Core engine initialization
static ppdb_error_t ppdb_engine_core_init(ppdb_engine_t* engine) {
    ppdb_error_t err;

    // Initialize statistics
    err = ppdb_engine_stats_init(&engine->stats);
    if (err != PPDB_OK) return err;

    return PPDB_OK;
}

// Core engine cleanup
static void ppdb_engine_core_cleanup(ppdb_engine_t* engine) {
    if (!engine) return;

    // Cleanup statistics
    ppdb_engine_stats_cleanup(&engine->stats);
}

// Core engine operations
static ppdb_error_t ppdb_engine_core_start(ppdb_engine_t* engine) {
    if (!engine) return PPDB_ENGINE_ERR_PARAM;

    // Start IO thread if needed
    if (!engine->io_mgr.io_running) {
        return ppdb_engine_io_init(engine);
    }

    return PPDB_OK;
}

static void ppdb_engine_core_stop(ppdb_engine_t* engine) {
    if (!engine) return;

    // Stop IO thread if running
    if (engine->io_mgr.io_running) {
        ppdb_engine_io_cleanup(engine);
    }
}