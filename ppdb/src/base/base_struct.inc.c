/*
 * base_struct.inc.c - Core data structure implementations for PPDB
 *
 * This file contains the fundamental data structure implementations used
 * throughout the PPDB system, including linked lists, hash tables, and other
 * basic data structures.
 *
 * Copyright (c) 2023 PPDB Authors
 */

#include <cosmopolitan.h>
#include "internal/base.h"

/* Basic data structures */

// Linked list node
typedef struct ppdb_base_list_node {
    struct ppdb_base_list_node *next;
    struct ppdb_base_list_node *prev;
    void *data;
} ppdb_base_list_node_t;

// Linked list
typedef struct ppdb_base_list {
    ppdb_base_list_node_t *head;
    ppdb_base_list_node_t *tail;
    size_t size;
} ppdb_base_list_t;

// Hash table entry
typedef struct ppdb_base_hash_entry {
    char *key;
    void *value;
    struct ppdb_base_hash_entry *next;
} ppdb_base_hash_entry_t;

// Hash table
typedef struct ppdb_base_hash_table {
    ppdb_base_hash_entry_t **buckets;
    size_t size;
    size_t capacity;
} ppdb_base_hash_table_t;

// Initialize a new linked list
ppdb_error_t ppdb_base_list_init(ppdb_base_list_t *list) {
    if (!list) {
        return PPDB_ERR_PARAM;
    }
    
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    
    return PPDB_OK;
}

// Add node to list
ppdb_error_t ppdb_base_list_add(ppdb_base_list_t *list, void *data) {
    if (!list) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_base_list_node_t *node = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_list_node_t));
    if (!node) {
        return PPDB_ERR_MEMORY;
    }
    
    node->data = data;
    node->next = NULL;
    node->prev = list->tail;
    
    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    
    list->tail = node;
    list->size++;
    
    return PPDB_OK;
}

// Remove node from list
void ppdb_base_list_remove(ppdb_base_list_t *list, ppdb_base_list_node_t *node) {
    if (!list || !node) {
        return;
    }
    
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }
    
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }
    
    list->size--;
    ppdb_base_aligned_free(node);
}

// Initialize hash table
ppdb_error_t ppdb_base_hash_init(ppdb_base_hash_table_t *table, size_t capacity) {
    if (!table || capacity == 0) {
        return PPDB_ERR_PARAM;
    }
    
    table->buckets = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_hash_entry_t*) * capacity);
    if (!table->buckets) {
        return PPDB_ERR_MEMORY;
    }
    
    memset(table->buckets, 0, sizeof(ppdb_base_hash_entry_t*) * capacity);
    table->capacity = capacity;
    table->size = 0;
    
    return PPDB_OK;
}

// Hash table cleanup
void ppdb_base_hash_cleanup(ppdb_base_hash_table_t *table) {
    if (!table) {
        return;
    }
    
    for (size_t i = 0; i < table->capacity; i++) {
        ppdb_base_hash_entry_t *entry = table->buckets[i];
        while (entry) {
            ppdb_base_hash_entry_t *next = entry->next;
            ppdb_base_aligned_free(entry->key);
            ppdb_base_aligned_free(entry);
            entry = next;
        }
    }
    
    ppdb_base_aligned_free(table->buckets);
    table->buckets = NULL;
    table->size = 0;
    table->capacity = 0;
}