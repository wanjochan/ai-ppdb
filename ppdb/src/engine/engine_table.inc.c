/*
 * engine_table.inc.c - Engine table implementation
 */

#include <cosmopolitan.h>
#include "internal/engine.h"

// Create a new table
ppdb_error_t ppdb_engine_table_create(ppdb_engine_t* engine, const char* name, ppdb_engine_table_t** table) {
    if (!engine || !name || !table) return PPDB_ENGINE_ERR_PARAM;

    // Lock engine
    ppdb_error_t err = ppdb_base_mutex_lock(engine->lock);
    if (err != PPDB_OK) return err;

    // Check if table already exists
    ppdb_engine_table_t* existing = NULL;
    err = ppdb_engine_table_list_find(engine->tables, name, &existing);
    if (err != PPDB_OK && err != PPDB_ENGINE_ERR_NOT_FOUND) {
        ppdb_base_mutex_unlock(engine->lock);
        return err;
    }
    if (existing) {
        ppdb_base_mutex_unlock(engine->lock);
        return PPDB_ENGINE_ERR_EXISTS;
    }

    // Allocate table structure
    ppdb_engine_table_t* new_table = malloc(sizeof(ppdb_engine_table_t));
    if (!new_table) {
        ppdb_base_mutex_unlock(engine->lock);
        return PPDB_ENGINE_ERR_MEMORY;
    }

    // Initialize table structure
    memset(new_table, 0, sizeof(ppdb_engine_table_t));
    new_table->name = strdup(name);
    if (!new_table->name) {
        free(new_table);
        ppdb_base_mutex_unlock(engine->lock);
        return PPDB_ENGINE_ERR_MEMORY;
    }
    new_table->name_len = strlen(name);
    new_table->engine = engine;
    new_table->entries = NULL;
    new_table->size = 0;
    new_table->is_open = true;

    // Create table mutex
    err = ppdb_base_mutex_create(&new_table->lock);
    if (err != PPDB_OK) {
        free(new_table->name);
        free(new_table);
        ppdb_base_mutex_unlock(engine->lock);
        return err;
    }

    // Add table to list
    err = ppdb_engine_table_list_add(engine->tables, new_table);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(new_table->lock);
        free(new_table->name);
        free(new_table);
        ppdb_base_mutex_unlock(engine->lock);
        return err;
    }

    *table = new_table;
    ppdb_base_mutex_unlock(engine->lock);
    return PPDB_OK;
}

// Open an existing table
ppdb_error_t ppdb_engine_table_open(ppdb_engine_t* engine, const char* name, ppdb_engine_table_t** table) {
    if (!engine || !name || !table) return PPDB_ENGINE_ERR_PARAM;

    // Lock engine
    ppdb_error_t err = ppdb_base_mutex_lock(engine->lock);
    if (err != PPDB_OK) return err;

    // Find table
    ppdb_engine_table_t* existing = NULL;
    err = ppdb_engine_table_list_find(engine->tables, name, &existing);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(engine->lock);
        return err;
    }

    // Check table state
    if (!existing->is_open) {
        ppdb_base_mutex_unlock(engine->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    *table = existing;
    ppdb_base_mutex_unlock(engine->lock);
    return PPDB_OK;
}

// Close a table
void ppdb_engine_table_close(ppdb_engine_table_t* table) {
    if (!table) return;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return;

    // Check table state
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        return;
    }

    // Mark table as closed
    table->is_open = false;

    ppdb_base_mutex_unlock(table->lock);
}

// Drop a table
ppdb_error_t ppdb_engine_table_drop(ppdb_engine_t* engine, const char* name) {
    if (!engine || !name) return PPDB_ENGINE_ERR_PARAM;

    // Lock engine
    ppdb_error_t err = ppdb_base_mutex_lock(engine->lock);
    if (err != PPDB_OK) return err;

    // Find table
    ppdb_engine_table_t* table = NULL;
    err = ppdb_engine_table_list_find(engine->tables, name, &table);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(engine->lock);
        return err;
    }

    // Lock table
    err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(engine->lock);
        return err;
    }

    // Check table state
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        ppdb_base_mutex_unlock(engine->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // Remove table from list
    err = ppdb_engine_table_list_remove(engine->tables, name);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(table->lock);
        ppdb_base_mutex_unlock(engine->lock);
        return err;
    }

    // Free table entries
    ppdb_engine_entry_t* entry = table->entries;
    while (entry) {
        ppdb_engine_entry_t* next = entry->next;
        free(entry->key);
        free(entry->value);
        free(entry);
        entry = next;
    }

    // Free table structure
    ppdb_base_mutex_unlock(table->lock);
    ppdb_base_mutex_destroy(table->lock);
    free(table->name);
    free(table);

    ppdb_base_mutex_unlock(engine->lock);
    return PPDB_OK;
}

// Compact a table
ppdb_error_t ppdb_engine_table_compact(ppdb_engine_table_t* table) {
    if (!table) return PPDB_ENGINE_ERR_PARAM;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Check table state
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // TODO: Implement table compaction
    ppdb_base_mutex_unlock(table->lock);
    return PPDB_OK;
}

// Cleanup expired entries
ppdb_error_t ppdb_engine_table_cleanup_expired(ppdb_engine_table_t* table) {
    if (!table) return PPDB_ENGINE_ERR_PARAM;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Check table state
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // TODO: Implement expired entry cleanup
    ppdb_base_mutex_unlock(table->lock);
    return PPDB_OK;
}

// Optimize table indexes
ppdb_error_t ppdb_engine_table_optimize_indexes(ppdb_engine_table_t* table) {
    if (!table) return PPDB_ENGINE_ERR_PARAM;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Check table state
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // TODO: Implement index optimization
    ppdb_base_mutex_unlock(table->lock);
    return PPDB_OK;
} 