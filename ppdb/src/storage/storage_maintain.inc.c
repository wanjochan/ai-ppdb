/*
 * storage_maintain.inc.c - Storage Maintenance Implementation
 */

#include <cosmopolitan.h>
#include "internal/storage.h"
#include "internal/engine.h"
#include "internal/base.h"

// Storage maintenance functions
ppdb_error_t ppdb_storage_maintain_init(ppdb_storage_t* storage) {
    if (!storage) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    // Initialize maintenance structure
    storage->maintain = malloc(sizeof(ppdb_storage_maintain_t));
    if (!storage->maintain) {
        return PPDB_STORAGE_ERR_MEMORY;
    }

    // Initialize maintenance mutex
    ppdb_error_t err = ppdb_base_mutex_create(&storage->maintain->mutex);
    if (err != PPDB_OK) {
        free(storage->maintain);
        storage->maintain = NULL;
        return err;
    }

    // Initialize maintenance state
    storage->maintain->is_running = false;
    storage->maintain->should_stop = false;
    storage->maintain->task = NULL;

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_maintain_cleanup(ppdb_storage_t* storage) {
    if (!storage || !storage->maintain) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    // Stop maintenance if running
    if (storage->maintain->is_running) {
        storage->maintain->should_stop = true;
        while (storage->maintain->is_running) {
            ppdb_engine_yield();
        }
    }

    // Cleanup maintenance mutex
    if (storage->maintain->mutex) {
        ppdb_base_mutex_destroy(storage->maintain->mutex);
    }

    // Free maintenance structure
    free(storage->maintain);
    storage->maintain = NULL;

    return PPDB_OK;
}

// 维护任务回调函数
static void maintenance_task(void* arg) {
    ppdb_storage_t* storage = (ppdb_storage_t*)arg;
    if (!storage) return;

    storage->maintain->is_running = true;

    while (!storage->maintain->should_stop) {
        // Start a write transaction for maintenance
        ppdb_engine_txn_t* tx = NULL;
        ppdb_error_t err = ppdb_engine_txn_begin(storage->engine, true, &tx);
        if (err != PPDB_OK) {
            continue;
        }

        // Lock maintenance mutex
        err = ppdb_base_mutex_lock(storage->maintain->mutex);
        if (err == PPDB_OK) {
            // Perform maintenance tasks
            ppdb_storage_maintain_compact(storage);
            ppdb_storage_maintain_cleanup_expired(storage);
            ppdb_storage_maintain_optimize_indexes(storage);

            ppdb_base_mutex_unlock(storage->maintain->mutex);
        }

        // Commit or rollback transaction
        if (err == PPDB_OK) {
            ppdb_engine_txn_commit(tx);
        } else {
            ppdb_engine_txn_rollback(tx);
        }

        // Sleep between maintenance cycles
        ppdb_base_sleep(1000);  // Sleep for 1 second
    }

    storage->maintain->is_running = false;
}

ppdb_error_t ppdb_storage_maintain_start(ppdb_storage_t* storage) {
    if (!storage) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    if (storage->maintain->is_running) {
        return PPDB_STORAGE_ERR_ALREADY_RUNNING;
    }

    // Schedule maintenance task
    ppdb_error_t err = ppdb_engine_async_schedule(storage->engine,
                                                maintenance_task,
                                                storage,
                                                &storage->maintain->task);
    if (err != PPDB_OK) {
        return err;
    }

    // Wait for task to start
    while (!storage->maintain->is_running) {
        ppdb_engine_yield();
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_maintain_stop(ppdb_storage_t* storage) {
    if (!storage) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    if (!storage->maintain->is_running) {
        return PPDB_STORAGE_ERR_NOT_RUNNING;
    }

    // Signal task to stop
    storage->maintain->should_stop = true;

    // Wait for task to stop
    while (storage->maintain->is_running) {
        ppdb_engine_yield();
    }

    // Cancel task
    if (storage->maintain->task) {
        ppdb_engine_async_cancel(storage->maintain->task);
        storage->maintain->task = NULL;
    }

    return PPDB_OK;
}

static void table_compact_fn(ppdb_engine_table_t* table, void* arg) {
    ppdb_error_t* err = (ppdb_error_t*)arg;
    if (*err != PPDB_OK || !table || !table->is_open) return;
    
    *err = ppdb_engine_table_compact(table);
}

static void table_cleanup_expired_fn(ppdb_engine_table_t* table, void* arg) {
    ppdb_error_t* err = (ppdb_error_t*)arg;
    if (*err != PPDB_OK || !table || !table->is_open) return;
    
    *err = ppdb_engine_table_cleanup_expired(table);
}

static void table_optimize_indexes_fn(ppdb_engine_table_t* table, void* arg) {
    ppdb_error_t* err = (ppdb_error_t*)arg;
    if (*err != PPDB_OK || !table || !table->is_open) return;
    
    *err = ppdb_engine_table_optimize_indexes(table);
}

ppdb_error_t ppdb_storage_maintain_compact(ppdb_storage_t* storage) {
    if (!storage) return PPDB_STORAGE_ERR_PARAM;

    // Lock maintenance mutex
    ppdb_error_t err = ppdb_base_mutex_lock(storage->maintain->mutex);
    if (err != PPDB_OK) return err;

    // Start a write transaction
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(storage->engine, true, &tx);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(storage->maintain->mutex);
        return err;
    }

    // Compact all tables
    ppdb_error_t compact_err = PPDB_OK;
    err = ppdb_engine_table_list_foreach(storage->tables, table_compact_fn, &compact_err);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(storage->maintain->mutex);
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    // Check for compaction errors
    if (compact_err != PPDB_OK) {
        ppdb_base_mutex_unlock(storage->maintain->mutex);
        ppdb_engine_txn_rollback(tx);
        return compact_err;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    ppdb_base_mutex_unlock(storage->maintain->mutex);
    return err;
}

ppdb_error_t ppdb_storage_maintain_cleanup_expired(ppdb_storage_t* storage) {
    if (!storage) return PPDB_STORAGE_ERR_PARAM;

    // Lock maintenance mutex
    ppdb_error_t err = ppdb_base_mutex_lock(storage->maintain->mutex);
    if (err != PPDB_OK) return err;

    // Start a write transaction
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(storage->engine, true, &tx);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(storage->maintain->mutex);
        return err;
    }

    // TODO: Implement cleanup of expired data

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    ppdb_base_mutex_unlock(storage->maintain->mutex);
    return err;
}

ppdb_error_t ppdb_storage_maintain_optimize_indexes(ppdb_storage_t* storage) {
    if (!storage) return PPDB_STORAGE_ERR_PARAM;

    // Lock maintenance mutex
    ppdb_error_t err = ppdb_base_mutex_lock(storage->maintain->mutex);
    if (err != PPDB_OK) return err;

    // Start a write transaction
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(storage->engine, true, &tx);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(storage->maintain->mutex);
        return err;
    }

    // TODO: Implement index optimization

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    ppdb_base_mutex_unlock(storage->maintain->mutex);
    return err;
}