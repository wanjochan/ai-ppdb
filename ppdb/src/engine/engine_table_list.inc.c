/*
 * engine_table_list.inc.c - Engine Table List Management Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"

// Table name comparison function
static int ppdb_engine_compare_table_name(const void* a, const void* b) {
    if (!a || !b) return a ? 1 : (b ? -1 : 0);
    const ppdb_engine_table_t* table_a = (const ppdb_engine_table_t*)a;
    const ppdb_engine_table_t* table_b = (const ppdb_engine_table_t*)b;
    return strcmp(table_a->name, table_b->name);
}

// Table list operations
ppdb_error_t ppdb_engine_table_list_create(ppdb_engine_t* engine, ppdb_engine_table_list_t** list) {
    if (!engine || !list) return PPDB_ENGINE_ERR_PARAM;
    if (*list) return PPDB_ENGINE_ERR_PARAM;  // Don't allow overwriting existing list

    // Allocate list structure
    ppdb_engine_table_list_t* new_list = malloc(sizeof(ppdb_engine_table_list_t));
    if (!new_list) return PPDB_ENGINE_ERR_MEMORY;

    // Initialize list structure
    memset(new_list, 0, sizeof(ppdb_engine_table_list_t));
    new_list->engine = engine;
    new_list->skiplist = NULL;

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

    *list = new_list;
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_table_list_destroy(ppdb_engine_table_list_t* list) {
    if (!list) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_base_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Destroy skiplist
    if (list->skiplist) {
        ppdb_base_skiplist_destroy(list->skiplist);
        list->skiplist = NULL;
    }

    // Unlock and destroy list mutex
    ppdb_base_mutex_unlock(list->lock);
    ppdb_base_mutex_destroy(list->lock);

    // Free list structure
    free(list);
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_table_list_add(ppdb_engine_table_list_t* list, ppdb_engine_table_t* table) {
    if (!list || !table) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_base_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Check if table already exists
    ppdb_engine_table_t* existing = NULL;
    err = ppdb_base_skiplist_find(list->skiplist, table->name, (void**)&existing);
    if (err == PPDB_OK && existing) {
        ppdb_base_mutex_unlock(list->lock);
        return PPDB_ENGINE_ERR_EXISTS;
    }

    // Add table to skiplist
    err = ppdb_base_skiplist_insert(list->skiplist, table->name, table);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(list->lock);
        return err;
    }

    // Unlock list
    ppdb_base_mutex_unlock(list->lock);

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_table_list_remove(ppdb_engine_table_list_t* list, const char* name) {
    if (!list || !name) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_base_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Remove table from skiplist
    err = ppdb_base_skiplist_remove(list->skiplist, name);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(list->lock);
        return err;
    }

    // Unlock list
    ppdb_base_mutex_unlock(list->lock);

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_table_list_find(ppdb_engine_table_list_t* list, const char* name,
                                     ppdb_engine_table_t** table) {
    if (!list || !name || !table) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_base_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Find table in skiplist
    err = ppdb_base_skiplist_find(list->skiplist, name, (void**)table);
    if (err != PPDB_OK) {
        *table = NULL;
    }

    // Unlock list
    ppdb_base_mutex_unlock(list->lock);

    return PPDB_OK;
}

// Iterate over tables in list
ppdb_error_t ppdb_engine_table_list_foreach(ppdb_engine_table_list_t* list, void (*fn)(ppdb_engine_table_t* table, void* arg), void* arg) {
    if (!list || !fn) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_engine_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Create iterator
    ppdb_base_skiplist_iterator_t* it = ppdb_base_skiplist_iterator_create(list->skiplist);
    if (!it) {
        ppdb_engine_mutex_unlock(list->lock);
        return PPDB_ENGINE_ERR_MEMORY;
    }

    // Iterate over tables
    while (ppdb_base_skiplist_iterator_valid(it)) {
        ppdb_engine_table_t* table = (ppdb_engine_table_t*)ppdb_base_skiplist_iterator_value(it);
        if (table) {
            fn(table, arg);
        }
        ppdb_base_skiplist_iterator_next(it);
    }

    // Cleanup iterator
    ppdb_base_skiplist_iterator_destroy(it);

    // Unlock list
    ppdb_engine_mutex_unlock(list->lock);
    return PPDB_OK;
}