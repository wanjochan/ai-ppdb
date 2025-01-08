/*
 * database_core.inc.c - Database core functionality implementation
 */

#include <cosmopolitan.h>
#include "internal/database.h"

// Core database initialization
static ppdb_error_t ppdb_database_core_init(ppdb_database_t* db) {
    ppdb_error_t err;

    // Initialize statistics
    err = ppdb_database_stats_init(&db->stats);
    if (err != PPDB_OK) return err;

    return PPDB_OK;
}

// Core database cleanup
static void ppdb_database_core_cleanup(ppdb_database_t* db) {
    if (!db) return;

    // Cleanup statistics
    ppdb_database_stats_cleanup(&db->stats);
}

// Core database operations
static ppdb_error_t ppdb_database_core_start(ppdb_database_t* db) {
    if (!db) return PPDB_DATABASE_ERR_PARAM;

    // Start IO thread if needed
    if (!db->io_mgr.io_running) {
        return ppdb_database_io_init(db);
    }

    return PPDB_OK;
}

static void ppdb_database_core_stop(ppdb_database_t* db) {
    if (!db) return;

    // Stop IO thread if running
    if (db->io_mgr.io_running) {
        ppdb_database_io_cleanup(db);
    }
}