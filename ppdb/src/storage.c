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
    ppdb_storage_t* s = malloc(sizeof(ppdb_storage_t));
    if (!s) return PPDB_STORAGE_ERR_MEMORY;

    // Initialize storage structure
    memset(s, 0, sizeof(ppdb_storage_t));
    s->engine = engine;
    s->config = *config;

    // Initialize lock
    err = ppdb_engine_mutex_create(&s->lock);
    if (err != PPDB_OK) {
        free(s);
        return err;
    }

    // Initialize tables list
    err = ppdb_engine_table_list_create(engine, &s->tables);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_destroy(s->lock);
        free(s);
        return err;
    }

    // Initialize statistics
    s->stats.reads = 0;
    s->stats.writes = 0;
    s->stats.flushes = 0;
    s->stats.compactions = 0;
    s->stats.cache_hits = 0;
    s->stats.cache_misses = 0;
    s->stats.wal_syncs = 0;

    // Initialize maintenance
    err = ppdb_storage_maintain_init(s);
    if (err != PPDB_OK) {
        ppdb_engine_table_list_destroy(s->tables);
        ppdb_engine_mutex_destroy(s->lock);
        free(s);
        return err;
    }

    *storage = s;
    return PPDB_OK;
}

void ppdb_storage_destroy(ppdb_storage_t* storage) {
    if (!storage) return;

    // Stop and cleanup maintenance
    ppdb_storage_maintain_cleanup(storage);

    // Cleanup tables
    if (storage->tables) {
        ppdb_engine_table_list_destroy(storage->tables);
        storage->tables = NULL;
    }

    // Cleanup lock
    if (storage->lock) {
        ppdb_engine_mutex_destroy(storage->lock);
        storage->lock = NULL;
    }

    free(storage);
}

void ppdb_storage_get_stats(ppdb_storage_t* storage, ppdb_storage_stats_t* stats) {
    if (!storage || !stats) return;

    *stats = storage->stats;
}

ppdb_error_t ppdb_storage_config_validate(const ppdb_storage_config_t* config) {
    if (!config) return PPDB_STORAGE_ERR_PARAM;

    // Validate configuration values
    if (config->memtable_size == 0 ||
        config->block_size == 0 ||
        config->cache_size == 0 ||
        config->write_buffer_size == 0 ||
        !config->data_dir) {
        return PPDB_STORAGE_ERR_CONFIG;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_config_init(ppdb_storage_config_t* config) {
    if (!config) return PPDB_STORAGE_ERR_PARAM;

    // Set default values
    config->memtable_size = PPDB_DEFAULT_MEMTABLE_SIZE;
    config->block_size = PPDB_DEFAULT_BLOCK_SIZE;
    config->cache_size = PPDB_DEFAULT_CACHE_SIZE;
    config->write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE;
    config->data_dir = PPDB_DEFAULT_DATA_DIR;
    config->use_compression = PPDB_DEFAULT_USE_COMPRESSION;
    config->sync_writes = PPDB_DEFAULT_SYNC_WRITES;

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get_config(ppdb_storage_t* storage, ppdb_storage_config_t* config) {
    if (!storage || !config) return PPDB_STORAGE_ERR_PARAM;

    *config = storage->config;
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_update_config(ppdb_storage_t* storage, const ppdb_storage_config_t* config) {
    if (!storage || !config) return PPDB_STORAGE_ERR_PARAM;

    // Validate new configuration
    ppdb_error_t err = ppdb_storage_config_validate(config);
    if (err != PPDB_OK) return err;

    // Update configuration
    storage->config = *config;
    return PPDB_OK;
}

const char* ppdb_storage_strerror(ppdb_error_t err) {
    switch (err) {
        case PPDB_STORAGE_ERR_PARAM:
            return "Invalid parameter";
        case PPDB_STORAGE_ERR_TABLE:
            return "Table operation failed";
        case PPDB_STORAGE_ERR_INDEX:
            return "Index operation failed";
        case PPDB_STORAGE_ERR_WAL:
            return "WAL operation failed";
        case PPDB_STORAGE_ERR_IO:
            return "IO operation failed";
        case PPDB_STORAGE_ERR_ALREADY_RUNNING:
            return "Storage is already running";
        case PPDB_STORAGE_ERR_NOT_RUNNING:
            return "Storage is not running";
        case PPDB_STORAGE_ERR_TABLE_EXISTS:
            return "Table already exists";
        case PPDB_STORAGE_ERR_TABLE_NOT_FOUND:
            return "Table not found";
        case PPDB_STORAGE_ERR_CONFIG:
            return "Configuration error";
        case PPDB_STORAGE_ERR_MEMORY:
            return "Memory allocation failed";
        case PPDB_STORAGE_ERR_INTERNAL:
            return "Internal error";
        case PPDB_STORAGE_ERR_NOT_FOUND:
            return "Resource not found";
        case PPDB_STORAGE_ERR_INVALID_STATE:
            return "Invalid state";
        case PPDB_STORAGE_ERR_BUFFER_FULL:
            return "Buffer is too small";
        default:
            return "Unknown storage error";
    }
}
