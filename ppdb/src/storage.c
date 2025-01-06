/*
 * storage.c - PPDB存储层实现
 *
 * 本文件是PPDB存储层的主入口，负责组织和初始化所有存储模块。
 */

#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"

// Include implementation files
#include "storage/storage_table.inc.c"
#include "storage/storage_index.inc.c"
#include "storage/storage_ops.inc.c"

// Storage initialization
ppdb_error_t ppdb_storage_init(ppdb_storage_t** storage, ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!storage || !base || !config) return PPDB_ERR_PARAM;
    if (*storage) return PPDB_ERR_PARAM;

    // Validate configuration
    PPDB_RETURN_IF_ERROR(ppdb_storage_config_validate(config));

    // Allocate storage structure
    ppdb_storage_t* s = ppdb_base_aligned_alloc(16, sizeof(ppdb_storage_t));
    if (!s) return PPDB_ERR_MEMORY;

    // Initialize storage structure
    memset(s, 0, sizeof(ppdb_storage_t));
    s->base = base;
    s->config = *config;

    // Initialize lock
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_init(&s->lock));

    // Initialize tables list
    ppdb_base_skiplist_t* tables_list;
    PPDB_RETURN_IF_ERROR(ppdb_base_skiplist_create(&tables_list, table_name_compare));
    s->tables = (ppdb_storage_table_t*)tables_list;

    // Initialize statistics
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

// Storage cleanup
void ppdb_storage_destroy(ppdb_storage_t* storage) {
    if (!storage) return;

    // Destroy tables
    ppdb_base_skiplist_destroy((ppdb_base_skiplist_t*)storage->tables);

    // Destroy statistics
    ppdb_base_counter_destroy(storage->stats.reads);
    ppdb_base_counter_destroy(storage->stats.writes);
    ppdb_base_counter_destroy(storage->stats.flushes);
    ppdb_base_counter_destroy(storage->stats.compactions);
    ppdb_base_counter_destroy(storage->stats.cache_hits);
    ppdb_base_counter_destroy(storage->stats.cache_misses);
    ppdb_base_counter_destroy(storage->stats.wal_syncs);

    // Destroy lock
    ppdb_base_spinlock_destroy(&storage->lock);

    // Free storage structure
    ppdb_base_aligned_free(storage);
}

// Get storage statistics
void ppdb_storage_get_stats(ppdb_storage_t* storage, ppdb_storage_stats_t* stats) {
    if (!storage || !stats) return;

    *stats = storage->stats;
}

// Configuration validation
ppdb_error_t ppdb_storage_config_validate(const ppdb_storage_config_t* config) {
    if (!config) return PPDB_ERR_PARAM;

    if (config->memtable_size == 0) return PPDB_ERR_CONFIG;
    if (config->block_size == 0) return PPDB_ERR_CONFIG;
    if (config->cache_size == 0) return PPDB_ERR_CONFIG;
    if (config->write_buffer_size == 0) return PPDB_ERR_CONFIG;
    if (!config->data_dir) return PPDB_ERR_CONFIG;

    return PPDB_OK;
}

// Configuration initialization
ppdb_error_t ppdb_storage_config_init(ppdb_storage_config_t* config) {
    if (!config) return PPDB_ERR_PARAM;

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
    if (!storage || !config) return PPDB_ERR_PARAM;

    *config = storage->config;
    return PPDB_OK;
}

// Update storage configuration
ppdb_error_t ppdb_storage_update_config(ppdb_storage_t* storage, const ppdb_storage_config_t* config) {
    if (!storage || !config) return PPDB_ERR_PARAM;

    // Validate new configuration
    PPDB_RETURN_IF_ERROR(ppdb_storage_config_validate(config));

    // Update configuration
    storage->config = *config;
    return PPDB_OK;
}
