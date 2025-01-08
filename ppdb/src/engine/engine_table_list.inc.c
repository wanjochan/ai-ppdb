/*
 * engine_table_list.inc.c - Engine table list implementation
 */

#include <cosmopolitan.h>
#include "internal/engine.h"

// Compare table names
static int ppdb_engine_compare_table_name(const void* a, const void* b) {
    const char* name_a = (const char*)a;
    const char* name_b = (const char*)b;
    return strcmp(name_a, name_b);
}

// Create table list
ppdb_error_t ppdb_engine_table_list_create(ppdb_engine_t* engine, ppdb_engine_table_list_t** list) {
    if (!engine || !list) return PPDB_ENGINE_ERR_PARAM;

    // Allocate list structure
    ppdb_engine_table_list_t* new_list = malloc(sizeof(ppdb_engine_table_list_t));
    if (!new_list) return PPDB_ENGINE_ERR_MEMORY;

    // Create list mutex
    ppdb_error_t err = ppdb_base_mutex_create(&new_list->lock);
    if (err != PPDB_OK) {
        free(new_list);
        return err;
    }

    // Create skiplist
    err = ppdb_base_skiplist_create(&new_list->skiplist, ppdb_engine_compare_table_name);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(new_list->lock);
        free(new_list);
        return err;
    }

    // Set engine reference
    new_list->engine = engine;

    *list = new_list;
    return PPDB_OK;
}

// Destroy table list
void ppdb_engine_table_list_destroy(ppdb_engine_table_list_t* list) {
    if (!list) return;

    // Lock list
    ppdb_error_t err = ppdb_base_mutex_lock(list->lock);
    if (err != PPDB_OK) return;

    // Destroy skiplist
    if (list->skiplist) {
        ppdb_base_skiplist_destroy(list->skiplist);
        list->skiplist = NULL;
    }

    // Unlock and destroy mutex
    ppdb_base_mutex_unlock(list->lock);
    ppdb_base_mutex_destroy(list->lock);

    // Free list structure
    free(list);
}

// Add table to list
ppdb_error_t ppdb_engine_table_list_add(ppdb_engine_table_list_t* list, ppdb_engine_table_t* table) {
    if (!list || !table || !table->name) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_base_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Check if table already exists
    ppdb_engine_table_t* existing = NULL;
    size_t value_size;
    err = ppdb_base_skiplist_find(list->skiplist, table->name, table->name_len, (void**)&existing, &value_size);
    if (err != PPDB_ENGINE_ERR_NOT_FOUND) {
        ppdb_base_mutex_unlock(list->lock);
        return (err == PPDB_OK) ? PPDB_ENGINE_ERR_EXISTS : err;
    }

    // Add table to skiplist
    err = ppdb_base_skiplist_insert(list->skiplist, table->name, table->name_len, table, sizeof(ppdb_engine_table_t*));
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(list->lock);
        return err;
    }

    ppdb_base_mutex_unlock(list->lock);
    return PPDB_OK;
}

// Remove table from list
ppdb_error_t ppdb_engine_table_list_remove(ppdb_engine_table_list_t* list, const char* name) {
    if (!list || !name) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_base_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Remove table from skiplist
    err = ppdb_base_skiplist_remove(list->skiplist, name, strlen(name));
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(list->lock);
        return err;
    }

    ppdb_base_mutex_unlock(list->lock);
    return PPDB_OK;
}

// Find table in list
ppdb_error_t ppdb_engine_table_list_find(ppdb_engine_table_list_t* list, const char* name, ppdb_engine_table_t** table) {
    if (!list || !name || !table) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_base_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Find table in skiplist
    size_t value_size;
    err = ppdb_base_skiplist_find(list->skiplist, name, strlen(name), (void**)table, &value_size);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(list->lock);
        return err;
    }

    ppdb_base_mutex_unlock(list->lock);
    return PPDB_OK;
}

// Iterate over tables
ppdb_error_t ppdb_engine_table_list_foreach(ppdb_engine_table_list_t* list, void (*fn)(ppdb_engine_table_t* table, void* arg), void* arg) {
    if (!list || !fn) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_base_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Create iterator
    ppdb_base_skiplist_iterator_t* it = NULL;
    err = ppdb_base_skiplist_iterator_create(list->skiplist, &it, false);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(list->lock);
        return err;
    }

    // Iterate over tables
    while (ppdb_base_skiplist_iterator_valid(it)) {
        void* value;
        size_t value_size;
        err = ppdb_base_skiplist_iterator_value(it, &value, &value_size);
        if (err != PPDB_OK) break;

        ppdb_engine_table_t* table = *(ppdb_engine_table_t**)value;
        fn(table, arg);

        ppdb_base_skiplist_iterator_next(it);
    }

    // Cleanup
    ppdb_base_skiplist_iterator_destroy(it);
    ppdb_base_mutex_unlock(list->lock);
    return PPDB_OK;
}