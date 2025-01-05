/*
 * base_skiplist.inc.c - Skip List Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Skip list node structure
typedef struct ppdb_base_skiplist_node {
    void* key;
    void* value;
    struct ppdb_base_skiplist_node** forward;
    int level;
} ppdb_base_skiplist_node_t;

// Skip list structure
typedef struct ppdb_base_skiplist {
    ppdb_base_skiplist_node_t* header;
    int level;
    size_t size;
    ppdb_base_compare_func_t compare;
} ppdb_base_skiplist_t;

// Create a new skip list
ppdb_error_t ppdb_base_skiplist_create(ppdb_base_skiplist_t** list, ppdb_base_compare_func_t compare) {
    if (!list || !compare) return PPDB_ERR_PARAM;

    ppdb_base_skiplist_t* new_list = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_skiplist_t));
    if (!new_list) return PPDB_ERR_MEMORY;

    new_list->header = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_skiplist_node_t));
    if (!new_list->header) {
        ppdb_base_aligned_free(new_list);
        return PPDB_ERR_MEMORY;
    }

    new_list->header->forward = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_skiplist_node_t*) * MAX_SKIPLIST_LEVEL);
    if (!new_list->header->forward) {
        ppdb_base_aligned_free(new_list->header);
        ppdb_base_aligned_free(new_list);
        return PPDB_ERR_MEMORY;
    }

    memset(new_list->header->forward, 0, sizeof(ppdb_base_skiplist_node_t*) * MAX_SKIPLIST_LEVEL);
    new_list->level = 0;
    new_list->size = 0;
    new_list->compare = compare;

    *list = new_list;
    return PPDB_OK;
}

// Destroy a skip list
void ppdb_base_skiplist_destroy(ppdb_base_skiplist_t* list) {
    if (!list) return;

    ppdb_base_skiplist_node_t* current = list->header->forward[0];
    while (current) {
        ppdb_base_skiplist_node_t* next = current->forward[0];
        ppdb_base_aligned_free(current->forward);
        ppdb_base_aligned_free(current);
        current = next;
    }

    ppdb_base_aligned_free(list->header->forward);
    ppdb_base_aligned_free(list->header);
    ppdb_base_aligned_free(list);
}

// Insert a key-value pair into the skip list
ppdb_error_t ppdb_base_skiplist_insert(ppdb_base_skiplist_t* list, void* key, void* value) {
    if (!list || !key) return PPDB_ERR_PARAM;

    ppdb_base_skiplist_node_t* update[MAX_SKIPLIST_LEVEL];
    ppdb_base_skiplist_node_t* current = list->header;

    // Find position to insert
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];

    // Key already exists
    if (current && list->compare(current->key, key) == 0) {
        current->value = value;
        return PPDB_OK;
    }

    // Generate random level
    int new_level = 0;
    while (new_level < MAX_SKIPLIST_LEVEL - 1 && (rand() & 1)) {
        new_level++;
    }

    if (new_level > list->level) {
        for (int i = list->level; i < new_level; i++) {
            update[i] = list->header;
        }
        list->level = new_level;
    }

    // Create new node
    ppdb_base_skiplist_node_t* new_node = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_skiplist_node_t));
    if (!new_node) return PPDB_ERR_MEMORY;

    new_node->forward = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_skiplist_node_t*) * MAX_SKIPLIST_LEVEL);
    if (!new_node->forward) {
        ppdb_base_aligned_free(new_node);
        return PPDB_ERR_MEMORY;
    }

    new_node->key = key;
    new_node->value = value;
    new_node->level = new_level;

    // Update forward pointers
    for (int i = 0; i < new_level; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    list->size++;
    return PPDB_OK;
}

// Find a value by key in the skip list
void* ppdb_base_skiplist_find(ppdb_base_skiplist_t* list, void* key) {
    if (!list || !key) return NULL;

    ppdb_base_skiplist_node_t* current = list->header;

    // Search from top level
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];

    // Found key
    if (current && list->compare(current->key, key) == 0) {
        return current->value;
    }

    return NULL;
}

// Remove a key-value pair from the skip list
ppdb_error_t ppdb_base_skiplist_remove(ppdb_base_skiplist_t* list, void* key) {
    if (!list || !key) return PPDB_ERR_PARAM;

    ppdb_base_skiplist_node_t* update[MAX_SKIPLIST_LEVEL];
    ppdb_base_skiplist_node_t* current = list->header;

    // Find position to remove
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];

    // Key not found
    if (!current || list->compare(current->key, key) != 0) {
        return PPDB_ERR_NOT_FOUND;
    }

    // Update forward pointers
    for (int i = 0; i < list->level; i++) {
        if (update[i]->forward[i] != current) {
            break;
        }
        update[i]->forward[i] = current->forward[i];
    }

    // Update level
    while (list->level > 0 && list->header->forward[list->level - 1] == NULL) {
        list->level--;
    }

    ppdb_base_aligned_free(current->forward);
    ppdb_base_aligned_free(current);
    list->size--;

    return PPDB_OK;
}

// Get the size of the skip list
size_t ppdb_base_skiplist_size(ppdb_base_skiplist_t* list) {
    return list ? list->size : 0;
} 