/*
 * storage_maintain.inc.c - Storage Maintenance Implementation
 */

#include <cosmopolitan.h>
#include "internal/storage.h"
#include "internal/engine.h"
#include "internal/base.h"

// Forward declarations for skiplist iterator
typedef struct ppdb_skiplist_iterator_s {
    ppdb_base_skiplist_t* list;
    struct ppdb_base_skiplist_node_s* current;
} ppdb_skiplist_iterator_t;

// Skiplist iterator functions
static ppdb_skiplist_iterator_t* ppdb_skiplist_iterator_create(ppdb_base_skiplist_t* list) {
    if (!list) return NULL;
    ppdb_skiplist_iterator_t* it = malloc(sizeof(ppdb_skiplist_iterator_t));
    if (!it) return NULL;
    it->list = list;
    it->current = list->header->forward[0];
    return it;
}

static bool ppdb_skiplist_iterator_valid(ppdb_skiplist_iterator_t* it) {
    return it && it->current != NULL;
}

static void* ppdb_skiplist_iterator_value(ppdb_skiplist_iterator_t* it) {
    return it && it->current ? it->current->value : NULL;
}

static void ppdb_skiplist_iterator_next(ppdb_skiplist_iterator_t* it) {
    if (it && it->current) {
        it->current = it->current->forward[0];
    }
}

static void ppdb_skiplist_iterator_destroy(ppdb_skiplist_iterator_t* it) {
    free(it);
}

// Storage maintenance functions
ppdb_error_t ppdb_storage_maintain_init(ppdb_storage_t* storage) {
    if (!storage) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    // Initialize maintenance mutex
    ppdb_error_t err = ppdb_engine_mutex_create(&storage->maintain.mutex);
    if (err != PPDB_OK) {
        return err;
    }

    // Initialize maintenance flags
    storage->maintain.is_running = false;
    storage->maintain.should_stop = false;
    storage->maintain.task = NULL;

    return PPDB_OK;
}

void ppdb_storage_maintain_cleanup(ppdb_storage_t* storage) {
    if (!storage) {
        return;
    }

    // Stop maintenance if running
    if (storage->maintain.is_running) {
        storage->maintain.should_stop = true;
        // Wait for maintenance to stop
        while (storage->maintain.is_running) {
            ppdb_engine_yield();
        }
    }

    // Cancel task if exists
    if (storage->maintain.task) {
        ppdb_engine_async_cancel(storage->maintain.task);
        storage->maintain.task = NULL;
    }

    // Cleanup maintenance mutex
    if (storage->maintain.mutex) {
        ppdb_engine_mutex_destroy(storage->maintain.mutex);
        storage->maintain.mutex = NULL;
    }
}

// 维护任务回调函数
static void maintenance_task(void* arg) {
    ppdb_storage_t* storage = (ppdb_storage_t*)arg;
    if (!storage) {
        return;
    }

    storage->maintain.is_running = true;

    while (!storage->maintain.should_stop) {
        // 开始事务
        ppdb_engine_txn_t* tx = NULL;
        ppdb_error_t err = ppdb_engine_txn_begin(storage->engine, &tx);
        if (err == PPDB_OK) {
            err = ppdb_engine_mutex_lock(storage->maintain.mutex);
            if (err == PPDB_OK) {
                // 执行维护任务
                ppdb_storage_maintain_compact(storage);
                ppdb_storage_maintain_cleanup_expired(storage);
                ppdb_storage_maintain_optimize_indexes(storage);

                ppdb_engine_mutex_unlock(storage->maintain.mutex);
            }

            // 提交或回滚事务
            if (err == PPDB_OK) {
                err = ppdb_engine_txn_commit(tx);
                if (err != PPDB_OK) {
                    ppdb_engine_txn_rollback(tx);
                }
            } else {
                ppdb_engine_txn_rollback(tx);
            }
        }

        // 等待下一个维护周期
        ppdb_engine_sleep(1000); // 1秒
    }

    storage->maintain.is_running = false;
}

ppdb_error_t ppdb_storage_maintain_start(ppdb_storage_t* storage) {
    if (!storage) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    if (storage->maintain.is_running) {
        return PPDB_STORAGE_ERR_ALREADY_RUNNING;
    }

    // Schedule maintenance task
    ppdb_error_t err = ppdb_engine_async_schedule(storage->engine,
                                                maintenance_task,
                                                storage,
                                                &storage->maintain.task);
    if (err != PPDB_OK) {
        return err;
    }

    // Wait for task to start
    while (!storage->maintain.is_running) {
        ppdb_engine_yield();
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_maintain_stop(ppdb_storage_t* storage) {
    if (!storage) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    if (!storage->maintain.is_running) {
        return PPDB_STORAGE_ERR_NOT_RUNNING;
    }

    // Signal task to stop
    storage->maintain.should_stop = true;

    // Wait for task to stop
    while (storage->maintain.is_running) {
        ppdb_engine_yield();
    }

    // Cancel task
    if (storage->maintain.task) {
        ppdb_engine_async_cancel(storage->maintain.task);
        storage->maintain.task = NULL;
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
    if (!storage) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    // Lock maintenance mutex
    ppdb_error_t err = ppdb_engine_mutex_lock(storage->maintain.mutex);
    if (err != PPDB_OK) {
        return err;
    }

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(storage->engine, &tx);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(storage->maintain.mutex);
        return err;
    }

    // Compact all tables
    ppdb_error_t compact_err = PPDB_OK;
    err = ppdb_engine_table_list_foreach(storage->tables, table_compact_fn, &compact_err);
    if (err != PPDB_OK || compact_err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        ppdb_engine_mutex_unlock(storage->maintain.mutex);
        return err != PPDB_OK ? err : compact_err;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
    }

    ppdb_engine_mutex_unlock(storage->maintain.mutex);
    return err;
}

ppdb_error_t ppdb_storage_maintain_cleanup_expired(ppdb_storage_t* storage) {
    if (!storage) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    // Lock maintenance mutex
    ppdb_error_t err = ppdb_engine_mutex_lock(storage->maintain.mutex);
    if (err != PPDB_OK) {
        return err;
    }

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(storage->engine, &tx);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(storage->maintain.mutex);
        return err;
    }

    // Cleanup expired data
    ppdb_error_t cleanup_err = PPDB_OK;
    err = ppdb_engine_table_list_foreach(storage->tables, table_cleanup_expired_fn, &cleanup_err);
    if (err != PPDB_OK || cleanup_err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        ppdb_engine_mutex_unlock(storage->maintain.mutex);
        return err != PPDB_OK ? err : cleanup_err;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
    }

    ppdb_engine_mutex_unlock(storage->maintain.mutex);
    return err;
}

ppdb_error_t ppdb_storage_maintain_optimize_indexes(ppdb_storage_t* storage) {
    if (!storage) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    // Lock maintenance mutex
    ppdb_error_t err = ppdb_engine_mutex_lock(storage->maintain.mutex);
    if (err != PPDB_OK) {
        return err;
    }

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(storage->engine, &tx);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(storage->maintain.mutex);
        return err;
    }

    // Optimize indexes
    ppdb_error_t optimize_err = PPDB_OK;
    err = ppdb_engine_table_list_foreach(storage->tables, table_optimize_indexes_fn, &optimize_err);
    if (err != PPDB_OK || optimize_err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        ppdb_engine_mutex_unlock(storage->maintain.mutex);
        return err != PPDB_OK ? err : optimize_err;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
    }

    ppdb_engine_mutex_unlock(storage->maintain.mutex);
    return err;
}