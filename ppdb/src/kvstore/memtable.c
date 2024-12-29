#include <cosmopolitan.h>
#include "internal/memtable.h"
#include "ppdb/error.h"
#include "ppdb/logger.h"
#include "internal/skiplist.h"
#include "internal/sync.h"
#include "internal/metrics.h"

// MemTable structure
struct ppdb_memtable_t {
    skiplist_t* skiplist;     // Underlying skip list
    ppdb_sync_t* sync;        // Synchronization primitives
    size_t max_size;          // Maximum size
    size_t current_size;      // Current size
    bool is_immutable;        // Whether immutable
    ppdb_metrics_t metrics;   // Performance monitoring
};

ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table) {
    if (!table) return PPDB_ERR_INVALID_ARG;

    ppdb_memtable_t* new_table = (ppdb_memtable_t*)malloc(sizeof(ppdb_memtable_t));
    if (!new_table) return PPDB_ERR_NO_MEMORY;

    // Create synchronization primitives
    ppdb_error_t err = ppdb_sync_create(&new_table->sync);
    if (err != PPDB_OK) {
        free(new_table);
        return err;
    }

    // Create skip list
    new_table->skiplist = skiplist_create();
    if (!new_table->skiplist) {
        ppdb_sync_destroy(new_table->sync);
        free(new_table);
        return PPDB_ERR_NO_MEMORY;
    }

    // Initialize performance monitoring
    ppdb_metrics_init(&new_table->metrics);

    new_table->max_size = size_limit;
    new_table->current_size = 0;
    new_table->is_immutable = false;

    *table = new_table;
    return PPDB_OK;
}

void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    if (!table) return;

    if (table->skiplist) {
        skiplist_destroy(table->skiplist);
        table->skiplist = NULL;
    }

    if (table->sync) {
        ppdb_sync_destroy(table->sync);
        table->sync = NULL;
    }

    // Destroy performance monitoring
    ppdb_metrics_destroy(&table->metrics);

    free(table);
}

ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table, const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len) {
    if (!table || !key || !value) return PPDB_ERR_INVALID_ARG;

    // Record operation start
    ppdb_metrics_begin_op(&table->metrics);
    
    // Check size limit
    if (ppdb_sync_load_size(table->sync, &table->current_size) >= table->max_size) {
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_FULL;
    }

    // Lock
    ppdb_sync_lock(table->sync);

    // Re-check size limit (may be written full by other threads while acquiring lock)
    if (ppdb_sync_load_size(table->sync, &table->current_size) >= table->max_size) {
        ppdb_sync_unlock(table->sync);
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_FULL;
    }

    // Check if immutable
    bool is_immutable;
    ppdb_sync_load_bool(table->sync, &table->is_immutable, &is_immutable);
    if (is_immutable) {
        ppdb_sync_unlock(table->sync);
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_IMMUTABLE;
    }

    // Insert data
    int ret = skiplist_put(table->skiplist, key, key_len, value, value_len);

    // Update size
    if (ret == 0) {
        ppdb_sync_add_size(table->sync, &table->current_size, key_len + value_len);
    }

    ppdb_sync_unlock(table->sync);
    
    // Record operation end
    ppdb_metrics_end_op(&table->metrics, key_len + value_len);
    
    return ret == 0 ? PPDB_OK : PPDB_ERR_INTERNAL;
}

ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table, const uint8_t* key, size_t key_len,
                              uint8_t** value, size_t* value_len) {
    if (!table || !key || !value || !value_len) return PPDB_ERR_INVALID_ARG;

    // Record operation start
    ppdb_metrics_begin_op(&table->metrics);
    
    ppdb_sync_lock(table->sync);

    // Find data
    int ret = skiplist_get(table->skiplist, key, key_len, *value, value_len);

    ppdb_sync_unlock(table->sync);
    
    // Record operation end
    ppdb_metrics_end_op(&table->metrics, 0);
    
    return ret == 0 ? PPDB_OK : PPDB_ERR_NOT_FOUND;
}

ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table, const uint8_t* key, size_t key_len) {
    if (!table || !key) return PPDB_ERR_INVALID_ARG;

    // Record operation start
    ppdb_metrics_begin_op(&table->metrics);
    
    ppdb_sync_lock(table->sync);

    // Check if immutable
    bool is_immutable;
    ppdb_sync_load_bool(table->sync, &table->is_immutable, &is_immutable);
    if (is_immutable) {
        ppdb_sync_unlock(table->sync);
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_IMMUTABLE;
    }

    // Delete data
    int ret = skiplist_delete(table->skiplist, key, key_len);

    ppdb_sync_unlock(table->sync);
    
    // Record operation end
    ppdb_metrics_end_op(&table->metrics, 0);
    
    return ret == 0 ? PPDB_OK : PPDB_ERR_NOT_FOUND;
}

size_t ppdb_memtable_size(ppdb_memtable_t* table) {
    if (!table) return 0;
    size_t size;
    ppdb_sync_load_size(table->sync, &table->current_size);
    return size;
}

size_t ppdb_memtable_max_size(ppdb_memtable_t* table) {
    if (!table) return 0;
    return table->max_size;
}

bool ppdb_memtable_is_immutable(ppdb_memtable_t* table) {
    if (!table) return false;
    bool is_immutable;
    ppdb_sync_load_bool(table->sync, &table->is_immutable, &is_immutable);
    return is_immutable;
}

void ppdb_memtable_set_immutable(ppdb_memtable_t* table) {
    if (!table) return;
    ppdb_sync_lock(table->sync);
    ppdb_sync_store_bool(table->sync, &table->is_immutable, true);
    ppdb_sync_unlock(table->sync);
}

ppdb_metrics_t* ppdb_memtable_get_metrics(ppdb_memtable_t* table) {
    if (!table) return NULL;
    return &table->metrics;
}
