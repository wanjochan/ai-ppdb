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

// Create table list
ppdb_error_t ppdb_engine_table_list_create(ppdb_engine_t* engine, ppdb_engine_table_list_t** list) {
    if (!engine || !list) return PPDB_ENGINE_ERR_PARAM;
    if (*list) return PPDB_ENGINE_ERR_PARAM;

    // Allocate list structure
    ppdb_engine_table_list_t* l = malloc(sizeof(ppdb_engine_table_list_t));
    if (!l) return PPDB_ENGINE_ERR_MEMORY;

    // Initialize structure
    memset(l, 0, sizeof(ppdb_engine_table_list_t));
    l->engine = engine;

    // Create mutex
    ppdb_error_t err = ppdb_engine_mutex_create(&l->lock);
    if (err != PPDB_OK) {
        free(l);
        return err;
    }

    // Create skiplist
    err = ppdb_base_skiplist_create(&l->skiplist, ppdb_engine_compare_table_name);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_destroy(l->lock);
        free(l);
        return err;
    }

    *list = l;
    return PPDB_OK;
}

// Destroy table list
ppdb_error_t ppdb_engine_table_list_destroy(ppdb_engine_table_list_t* list) {
    if (!list) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_engine_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Destroy skiplist
    if (list->skiplist) {
        ppdb_base_skiplist_destroy(list->skiplist);
        list->skiplist = NULL;
    }

    // Unlock and destroy mutex
    ppdb_engine_mutex_unlock(list->lock);
    ppdb_engine_mutex_destroy(list->lock);

    // Free structure
    free(list);
    return PPDB_OK;
}

// Add table to list
ppdb_error_t ppdb_engine_table_list_add(ppdb_engine_table_list_t* list, ppdb_engine_table_t* table) {
    if (!list || !table) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_engine_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Check if table already exists
    ppdb_engine_table_t* existing = NULL;
    err = ppdb_base_skiplist_find(list->skiplist, table, (void**)&existing);
    if (err == PPDB_OK) {
        ppdb_engine_mutex_unlock(list->lock);
        return PPDB_ENGINE_ERR_EXISTS;
    }

    // Add table to skiplist
    err = ppdb_base_skiplist_insert(list->skiplist, table, table);
    
    // Unlock list
    ppdb_engine_mutex_unlock(list->lock);
    return err;
}

// Remove table from list
ppdb_error_t ppdb_engine_table_list_remove(ppdb_engine_table_list_t* list, const char* name) {
    if (!list || !name) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_engine_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Create temporary table for lookup
    ppdb_engine_table_t temp = {
        .name = (char*)name,
        .name_len = strlen(name)
    };

    // Remove table from skiplist
    err = ppdb_base_skiplist_remove(list->skiplist, &temp);

    // Unlock list
    ppdb_engine_mutex_unlock(list->lock);
    return err;
}

// Find table in list
ppdb_error_t ppdb_engine_table_list_find(ppdb_engine_table_list_t* list, const char* name, ppdb_engine_table_t** table) {
    if (!list || !name || !table) return PPDB_ENGINE_ERR_PARAM;

    // Lock list
    ppdb_error_t err = ppdb_engine_mutex_lock(list->lock);
    if (err != PPDB_OK) return err;

    // Create temporary table for lookup
    ppdb_engine_table_t temp = {
        .name = (char*)name,
        .name_len = strlen(name)
    };

    // Find table in skiplist
    err = ppdb_base_skiplist_find(list->skiplist, &temp, (void**)table);

    // Unlock list
    ppdb_engine_mutex_unlock(list->lock);
    return err;
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