/*
 * storage.c - PPDB存储层实现
 *
 * 本文件是PPDB存储层的主入口，负责组织和初始化所有存储模块。
 */

#include <cosmopolitan.h>
#include "internal/engine.h"
#include "internal/storage.h"

// Include implementation files
#include "storage/storage_table.inc.c"
#include "storage/storage_index.inc.c"
#include "storage/storage_ops.inc.c"
#include "storage/storage_maintain.inc.c"
#include "storage/storage_wal.inc.c"

// Table name comparison function
static int ppdb_storage_compare_table_name(const void* a, const void* b) {
    if (!a || !b) return a ? 1 : (b ? -1 : 0);
    return strcmp((const char*)a, (const char*)b);
}

// Storage initialization
ppdb_error_t ppdb_storage_init(ppdb_storage_t** storage, ppdb_engine_t* engine, const ppdb_storage_config_t* config) {
    if (!storage || !engine || !config) return PPDB_STORAGE_ERR_PARAM;
    if (*storage) return PPDB_STORAGE_ERR_PARAM;

    // Validate configuration
    ppdb_error_t err = ppdb_storage_config_validate(config);
    if (err != PPDB_OK) return PPDB_STORAGE_ERR_PARAM;

    // Allocate storage structure
    ppdb_storage_t* s = ppdb_engine_malloc(sizeof(ppdb_storage_t));
    if (!s) return PPDB_STORAGE_ERR_MEMORY;

    // Initialize storage structure
    memset(s, 0, sizeof(ppdb_storage_t));
    s->engine = engine;
    s->config = *config;

    // Initialize lock
    err = ppdb_engine_mutex_create(&s->lock);
    if (err != PPDB_OK) {
        ppdb_engine_free(s);
        return err;
    }

    // Initialize tables list
    ppdb_engine_table_t* tables_list = NULL;
    err = ppdb_engine_table_create(engine, "tables", &tables_list);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_destroy(s->lock);
        ppdb_engine_free(s);
        return err;
    }
    s->tables = tables_list;

    // Initialize statistics
    err = ppdb_engine_counter_create(&s->stats.reads);
    if (err != PPDB_OK) {
        ppdb_engine_table_destroy(tables_list);
        ppdb_engine_mutex_destroy(s->lock);
        ppdb_engine_free(s);
        return err;
    }

    err = ppdb_engine_counter_create(&s->stats.writes);
    if (err != PPDB_OK) {
        ppdb_engine_counter_destroy(s->stats.reads);
        ppdb_engine_table_destroy(tables_list);
        ppdb_engine_mutex_destroy(s->lock);
        ppdb_engine_free(s);
        return err;
    }

    err = ppdb_engine_counter_create(&s->stats.flushes);
    if (err != PPDB_OK) {
        ppdb_engine_counter_destroy(s->stats.writes);
        ppdb_engine_counter_destroy(s->stats.reads);
        ppdb_engine_table_destroy(tables_list);
        ppdb_engine_mutex_destroy(s->lock);
        ppdb_engine_free(s);
        return err;
    }

    err = ppdb_engine_counter_create(&s->stats.compactions);
    if (err != PPDB_OK) {
        ppdb_engine_counter_destroy(s->stats.flushes);
        ppdb_engine_counter_destroy(s->stats.writes);
        ppdb_engine_counter_destroy(s->stats.reads);
        ppdb_engine_table_destroy(tables_list);
        ppdb_engine_mutex_destroy(s->lock);
        ppdb_engine_free(s);
        return err;
    }

    err = ppdb_engine_counter_create(&s->stats.cache_hits);
    if (err != PPDB_OK) {
        ppdb_engine_counter_destroy(s->stats.compactions);
        ppdb_engine_counter_destroy(s->stats.flushes);
        ppdb_engine_counter_destroy(s->stats.writes);
        ppdb_engine_counter_destroy(s->stats.reads);
        ppdb_engine_table_destroy(tables_list);
        ppdb_engine_mutex_destroy(s->lock);
        ppdb_engine_free(s);
        return err;
    }

    err = ppdb_engine_counter_create(&s->stats.cache_misses);
    if (err != PPDB_OK) {
        ppdb_engine_counter_destroy(s->stats.cache_hits);
        ppdb_engine_counter_destroy(s->stats.compactions);
        ppdb_engine_counter_destroy(s->stats.flushes);
        ppdb_engine_counter_destroy(s->stats.writes);
        ppdb_engine_counter_destroy(s->stats.reads);
        ppdb_engine_table_destroy(tables_list);
        ppdb_engine_mutex_destroy(s->lock);
        ppdb_engine_free(s);
        return err;
    }

    err = ppdb_engine_counter_create(&s->stats.wal_syncs);
    if (err != PPDB_OK) {
        ppdb_engine_counter_destroy(s->stats.cache_misses);
        ppdb_engine_counter_destroy(s->stats.cache_hits);
        ppdb_engine_counter_destroy(s->stats.compactions);
        ppdb_engine_counter_destroy(s->stats.flushes);
        ppdb_engine_counter_destroy(s->stats.writes);
        ppdb_engine_counter_destroy(s->stats.reads);
        ppdb_engine_table_destroy(tables_list);
        ppdb_engine_mutex_destroy(s->lock);
        ppdb_engine_free(s);
        return err;
    }

    *storage = s;
    return PPDB_OK;
}

// Storage cleanup
void ppdb_storage_destroy(ppdb_storage_t* storage) {
    if (!storage) return;

    // Destroy tables
    ppdb_engine_table_destroy(storage->tables);

    // Destroy statistics
    ppdb_engine_counter_destroy(storage->stats.reads);
    ppdb_engine_counter_destroy(storage->stats.writes);
    ppdb_engine_counter_destroy(storage->stats.flushes);
    ppdb_engine_counter_destroy(storage->stats.compactions);
    ppdb_engine_counter_destroy(storage->stats.cache_hits);
    ppdb_engine_counter_destroy(storage->stats.cache_misses);
    ppdb_engine_counter_destroy(storage->stats.wal_syncs);

    // Destroy lock
    ppdb_engine_mutex_destroy(storage->lock);

    // Free storage structure
    ppdb_engine_free(storage);
}

// Get storage statistics
void ppdb_storage_get_stats(ppdb_storage_t* storage, ppdb_storage_stats_t* stats) {
    if (!storage || !stats) return;

    *stats = storage->stats;
}

// Configuration validation
ppdb_error_t ppdb_storage_config_validate(const ppdb_storage_config_t* config) {
    if (!config) return PPDB_STORAGE_ERR_PARAM;

    if (config->memtable_size == 0) return PPDB_STORAGE_ERR_PARAM;
    if (config->block_size == 0) return PPDB_STORAGE_ERR_PARAM;
    if (config->cache_size == 0) return PPDB_STORAGE_ERR_PARAM;
    if (config->write_buffer_size == 0) return PPDB_STORAGE_ERR_PARAM;

    return PPDB_OK;
}

// Configuration initialization
ppdb_error_t ppdb_storage_config_init(ppdb_storage_config_t* config) {
    if (!config) return PPDB_STORAGE_ERR_PARAM;

    config->memtable_size = PPDB_DEFAULT_MEMTABLE_SIZE;
    config->block_size = PPDB_DEFAULT_BLOCK_SIZE;
    config->cache_size = PPDB_DEFAULT_CACHE_SIZE;
    config->write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE;
    config->data_dir = PPDB_DEFAULT_DATA_DIR;
    config->use_compression = PPDB_DEFAULT_USE_COMPRESSION;
    config->sync_writes = PPDB_DEFAULT_SYNC_WRITES;

    return PPDB_OK;
}

// Get storage configuration
ppdb_error_t ppdb_storage_get_config(ppdb_storage_t* storage, ppdb_storage_config_t* config) {
    if (!storage || !config) return PPDB_STORAGE_ERR_PARAM;

    *config = storage->config;
    return PPDB_OK;
}

// Update storage configuration
ppdb_error_t ppdb_storage_update_config(ppdb_storage_t* storage, const ppdb_storage_config_t* config) {
    if (!storage || !config) return PPDB_STORAGE_ERR_PARAM;

    // Validate new configuration
    PPDB_RETURN_IF_ERROR(ppdb_storage_config_validate(config));

    // Update configuration
    storage->config = *config;
    return PPDB_OK;
}
