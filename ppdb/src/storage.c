/*
 * storage.c - PPDB存储层实现
 *
 * 本文件是PPDB存储层的主入口，负责组织和初始化所有存储模块。
 */

#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"
#include "internal/storage.h"

// Internal storage structures
struct ppdb_storage_s {
    ppdb_base_t* base;                    // Base layer instance
    ppdb_storage_config_t config;         // Storage configuration
    ppdb_storage_stats_t stats;           // Storage statistics
    ppdb_base_mutex_t* mutex;             // Global storage mutex
    ppdb_base_skiplist_t* tables;         // Table metadata index
};

struct ppdb_storage_table_s {
    char* name;                           // Table name
    ppdb_storage_t* storage;              // Parent storage instance
    ppdb_base_skiplist_t* memtable;       // In-memory table
    ppdb_base_skiplist_t* indexes;        // Index metadata
    ppdb_base_mutex_t* mutex;             // Table-level mutex
};

struct ppdb_storage_index_s {
    char* name;                           // Index name
    ppdb_storage_table_t* table;          // Parent table
    ppdb_base_skiplist_t* index;          // Index data structure
    ppdb_base_mutex_t* mutex;             // Index-level mutex
};

// Include storage implementation files
#include "storage/storage_table.inc.c"
#include "storage/storage_index.inc.c"
#include "storage/storage_ops.inc.c"

//-----------------------------------------------------------------------------
// Storage initialization and cleanup
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_storage_init(ppdb_storage_t** storage, ppdb_base_t* base, const ppdb_storage_config_t* config) {
    PPDB_CHECK_NULL(storage);
    PPDB_CHECK_NULL(base);
    PPDB_CHECK_NULL(config);

    // Allocate storage structure
    ppdb_storage_t* s = ppdb_base_aligned_alloc(sizeof(ppdb_storage_t));
    if (!s) return PPDB_BASE_ERR_MEMORY;

    // Initialize storage
    s->base = base;
    s->config = *config;
    
    // Create mutex
    PPDB_RETURN_IF_ERROR(ppdb_base_mutex_create(&s->mutex));

    // Create table index
    PPDB_RETURN_IF_ERROR(ppdb_base_skiplist_create(&s->tables, NULL));

    // Initialize statistics counters
    PPDB_RETURN_IF_ERROR(ppdb_base_counter_create(&s->stats.reads));
    PPDB_RETURN_IF_ERROR(ppdb_base_counter_create(&s->stats.writes));
    PPDB_RETURN_IF_ERROR(ppdb_base_counter_create(&s->stats.flushes));
    PPDB_RETURN_IF_ERROR(ppdb_base_counter_create(&s->stats.compactions));
    PPDB_RETURN_IF_ERROR(ppdb_base_counter_create(&s->stats.cache_hits));
    PPDB_RETURN_IF_ERROR(ppdb_base_counter_create(&s->stats.cache_misses));
    PPDB_RETURN_IF_ERROR(ppdb_base_counter_create(&s->stats.wal_syncs));

    *storage = s;
    return PPDB_OK;
}

void ppdb_storage_destroy(ppdb_storage_t* storage) {
    if (!storage) return;

    // Cleanup statistics counters
    ppdb_base_counter_destroy(storage->stats.reads);
    ppdb_base_counter_destroy(storage->stats.writes);
    ppdb_base_counter_destroy(storage->stats.flushes);
    ppdb_base_counter_destroy(storage->stats.compactions);
    ppdb_base_counter_destroy(storage->stats.cache_hits);
    ppdb_base_counter_destroy(storage->stats.cache_misses);
    ppdb_base_counter_destroy(storage->stats.wal_syncs);

    // Cleanup table index
    ppdb_base_skiplist_destroy(storage->tables);

    // Cleanup mutex
    ppdb_base_mutex_destroy(storage->mutex);

    // Free storage structure
    ppdb_base_aligned_free(storage);
}

void ppdb_storage_get_stats(ppdb_storage_t* storage, ppdb_storage_stats_t* stats) {
    if (!storage || !stats) return;
    *stats = storage->stats;
}
