/*
 * base_skiplist.inc.c - Skiplist Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Create skiplist iterator
ppdb_base_skiplist_iterator_t* ppdb_base_skiplist_iterator_create(ppdb_base_skiplist_t* list) {
    if (!list) return NULL;
    ppdb_base_skiplist_iterator_t* it = malloc(sizeof(ppdb_base_skiplist_iterator_t));
    if (!it) return NULL;
    it->list = list;
    it->current = list->header->forward[0];
    return it;
}

// Destroy skiplist iterator
void ppdb_base_skiplist_iterator_destroy(ppdb_base_skiplist_iterator_t* it) {
    if (it) {
        free(it);
    }
}

// Check if iterator is valid
bool ppdb_base_skiplist_iterator_valid(ppdb_base_skiplist_iterator_t* it) {
    return it && it->current != NULL;
}

// Get value at current iterator position
void* ppdb_base_skiplist_iterator_value(ppdb_base_skiplist_iterator_t* it) {
    return it && it->current ? it->current->value : NULL;
}

// Move iterator to next position
void ppdb_base_skiplist_iterator_next(ppdb_base_skiplist_iterator_t* it) {
    if (it && it->current) {
        it->current = it->current->forward[0];
    }
}

// Create skiplist
ppdb_error_t ppdb_base_skiplist_create(ppdb_base_skiplist_t** list, ppdb_base_compare_func_t compare) {
    if (!list || !compare) return PPDB_BASE_ERR_PARAM;

    ppdb_base_skiplist_t* new_list = malloc(sizeof(ppdb_base_skiplist_t));
    if (!new_list) return PPDB_BASE_ERR_MEMORY;

    new_list->level = 1;
    new_list->size = 0;
    new_list->compare = compare;

    // Create header node
    new_list->header = malloc(sizeof(ppdb_base_skiplist_node_t));
    if (!new_list->header) {
        free(new_list);
        return PPDB_BASE_ERR_MEMORY;
    }

    new_list->header->forward = malloc(PPDB_MAX_SKIPLIST_LEVEL * sizeof(ppdb_base_skiplist_node_t*));
    if (!new_list->header->forward) {
        free(new_list->header);
        free(new_list);
        return PPDB_BASE_ERR_MEMORY;
    }

    // Initialize header node
    new_list->header->key = NULL;
    new_list->header->value = NULL;
    new_list->header->level = PPDB_MAX_SKIPLIST_LEVEL;
    for (int i = 0; i < PPDB_MAX_SKIPLIST_LEVEL; i++) {
        new_list->header->forward[i] = NULL;
    }

    *list = new_list;
    return PPDB_OK;
}

// Destroy skiplist
ppdb_error_t ppdb_base_skiplist_destroy(ppdb_base_skiplist_t* list) {
    if (!list) return PPDB_BASE_ERR_PARAM;

    ppdb_base_skiplist_node_t* current = list->header->forward[0];
    while (current) {
        ppdb_base_skiplist_node_t* next = current->forward[0];
        free(current->forward);
        free(current);
        current = next;
    }

    free(list->header->forward);
    free(list->header);
    free(list);
    return PPDB_OK;
}

// Insert into skiplist
ppdb_error_t ppdb_base_skiplist_insert(ppdb_base_skiplist_t* list, const void* key, void* value) {
    if (!list || !key) return PPDB_BASE_ERR_PARAM;

    ppdb_base_skiplist_node_t* update[PPDB_MAX_SKIPLIST_LEVEL];
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
    int new_level = 1;
    while (rand() % 2 && new_level < PPDB_MAX_SKIPLIST_LEVEL) {
        new_level++;
    }

    // Update list level
    if (new_level > list->level) {
        for (int i = list->level; i < new_level; i++) {
            update[i] = list->header;
        }
        list->level = new_level;
    }

    // Create new node
    ppdb_base_skiplist_node_t* new_node = malloc(sizeof(ppdb_base_skiplist_node_t));
    if (!new_node) return PPDB_BASE_ERR_MEMORY;

    new_node->forward = malloc(new_level * sizeof(ppdb_base_skiplist_node_t*));
    if (!new_node->forward) {
        free(new_node);
        return PPDB_BASE_ERR_MEMORY;
    }

    new_node->key = key;
    new_node->value = value;
    new_node->level = new_level;

    // Insert node
    for (int i = 0; i < new_level; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    list->size++;
    return PPDB_OK;
}

// Find in skiplist
ppdb_error_t ppdb_base_skiplist_find(ppdb_base_skiplist_t* list, const void* key, void** value) {
    if (!list || !key || !value) return PPDB_BASE_ERR_PARAM;

    ppdb_base_skiplist_node_t* current = list->header;

    // Search for key
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];

    // Check if key found
    if (current && list->compare(current->key, key) == 0) {
        *value = current->value;
        return PPDB_OK;
    }

    return PPDB_BASE_ERR_NOT_FOUND;
}

// Remove from skiplist
ppdb_error_t ppdb_base_skiplist_remove(ppdb_base_skiplist_t* list, const void* key) {
    if (!list || !key) return PPDB_BASE_ERR_PARAM;

    ppdb_base_skiplist_node_t* update[PPDB_MAX_SKIPLIST_LEVEL];
    ppdb_base_skiplist_node_t* current = list->header;

    // Find node to remove
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];

    // Remove node if found
    if (current && list->compare(current->key, key) == 0) {
        for (int i = 0; i < list->level; i++) {
            if (update[i]->forward[i] != current) {
                break;
            }
            update[i]->forward[i] = current->forward[i];
        }

        // Update list level
        while (list->level > 1 && list->header->forward[list->level - 1] == NULL) {
            list->level--;
        }

        free(current->forward);
        free(current);
        list->size--;
        return PPDB_OK;
    }

    return PPDB_BASE_ERR_NOT_FOUND;
}

// Get skiplist size
size_t ppdb_base_skiplist_size(const ppdb_base_skiplist_t* list) {
    return list ? list->size : 0;
}

// Clear skiplist
void ppdb_base_skiplist_clear(ppdb_base_skiplist_t* list) {
    if (!list) return;

    ppdb_base_skiplist_node_t* current = list->header->forward[0];
    while (current) {
        ppdb_base_skiplist_node_t* next = current->forward[0];
        free(current->forward);
        free(current);
        current = next;
    }

    for (int i = 0; i < PPDB_MAX_SKIPLIST_LEVEL; i++) {
        list->header->forward[i] = NULL;
    }

    list->level = 1;
    list->size = 0;
} 