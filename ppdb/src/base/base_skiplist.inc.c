/*
 * base_skiplist.inc.c - Skiplist Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Create skiplist iterator
ppdb_error_t ppdb_base_skiplist_iterator_create(ppdb_base_skiplist_t* list, ppdb_base_skiplist_iterator_t** iterator, bool reverse) {
    if (!list || !iterator) return PPDB_BASE_ERR_PARAM;
    
    ppdb_base_skiplist_iterator_t* it = malloc(sizeof(ppdb_base_skiplist_iterator_t));
    if (!it) return PPDB_BASE_ERR_MEMORY;
    
    it->list = list;
    it->current = list->head->forward[0];
    it->reverse = reverse;
    
    *iterator = it;
    return PPDB_OK;
}

// Destroy skiplist iterator
ppdb_error_t ppdb_base_skiplist_iterator_destroy(ppdb_base_skiplist_iterator_t* it) {
    if (!it) return PPDB_BASE_ERR_PARAM;
    free(it);
    return PPDB_OK;
}

// Check if iterator is valid
bool ppdb_base_skiplist_iterator_valid(ppdb_base_skiplist_iterator_t* it) {
    return it && it->current != NULL;
}

// Get value at current iterator position
ppdb_error_t ppdb_base_skiplist_iterator_value(ppdb_base_skiplist_iterator_t* iterator, void** value, size_t* value_size) {
    if (!iterator || !value || !value_size) return PPDB_BASE_ERR_PARAM;
    if (!iterator->current) return PPDB_BASE_ERR_NOT_FOUND;
    
    *value = iterator->current->value;
    *value_size = iterator->current->value_size;
    return PPDB_OK;
}

// Move iterator to next position
ppdb_error_t ppdb_base_skiplist_iterator_next(ppdb_base_skiplist_iterator_t* it) {
    if (!it) return PPDB_BASE_ERR_PARAM;
    if (it->current) {
        it->current = it->current->forward[0];
    }
    return PPDB_OK;
}

// Create skiplist
ppdb_error_t ppdb_base_skiplist_create(ppdb_base_skiplist_t** list, ppdb_base_compare_func_t compare) {
    if (!list || !compare) return PPDB_BASE_ERR_PARAM;

    ppdb_base_skiplist_t* new_list = malloc(sizeof(ppdb_base_skiplist_t));
    if (!new_list) return PPDB_BASE_ERR_MEMORY;

    new_list->max_level = 1;
    new_list->size = 0;
    new_list->compare = compare;

    // Create header node
    new_list->head = malloc(sizeof(ppdb_base_skiplist_node_t));
    if (!new_list->head) {
        free(new_list);
        return PPDB_BASE_ERR_MEMORY;
    }

    new_list->head->forward = malloc(PPDB_MAX_SKIPLIST_LEVEL * sizeof(ppdb_base_skiplist_node_t*));
    if (!new_list->head->forward) {
        free(new_list->head);
        free(new_list);
        return PPDB_BASE_ERR_MEMORY;
    }

    // Initialize header node
    new_list->head->key = NULL;
    new_list->head->value = NULL;
    new_list->head->level = PPDB_MAX_SKIPLIST_LEVEL;
    for (int i = 0; i < PPDB_MAX_SKIPLIST_LEVEL; i++) {
        new_list->head->forward[i] = NULL;
    }

    *list = new_list;
    return PPDB_OK;
}

// Destroy skiplist
ppdb_error_t ppdb_base_skiplist_destroy(ppdb_base_skiplist_t* list) {
    if (!list) return PPDB_BASE_ERR_PARAM;

    ppdb_base_skiplist_node_t* current = list->head->forward[0];
    while (current) {
        ppdb_base_skiplist_node_t* next = current->forward[0];
        free(current->forward);
        free(current);
        current = next;
    }

    free(list->head->forward);
    free(list->head);
    free(list);
    return PPDB_OK;
}

// Insert into skiplist
ppdb_error_t ppdb_base_skiplist_insert(ppdb_base_skiplist_t* list, const void* key, size_t key_size, const void* value, size_t value_size) {
    if (!list || !key) return PPDB_BASE_ERR_PARAM;

    ppdb_base_skiplist_node_t* update[PPDB_MAX_SKIPLIST_LEVEL];
    ppdb_base_skiplist_node_t* current = list->head;

    // Find position to insert
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];

    // Key already exists
    if (current && list->compare(current->key, key) == 0) {
        // Update value
        void* new_value = malloc(value_size);
        if (!new_value) return PPDB_BASE_ERR_MEMORY;
        memcpy(new_value, value, value_size);
        free(current->value);
        current->value = new_value;
        current->value_size = value_size;
        return PPDB_OK;
    }

    // Generate random level
    int new_level = 1;
    while (rand() % 2 && new_level < PPDB_MAX_SKIPLIST_LEVEL) {
        new_level++;
    }

    // Update list level
    if (new_level > list->max_level) {
        for (int i = list->max_level; i < new_level; i++) {
            update[i] = list->head;
        }
        list->max_level = new_level;
    }

    // Create new node
    ppdb_base_skiplist_node_t* new_node = malloc(sizeof(ppdb_base_skiplist_node_t));
    if (!new_node) return PPDB_BASE_ERR_MEMORY;

    new_node->forward = malloc(new_level * sizeof(ppdb_base_skiplist_node_t*));
    if (!new_node->forward) {
        free(new_node);
        return PPDB_BASE_ERR_MEMORY;
    }

    // Copy key and value
    void* new_key = malloc(key_size);
    void* new_value = malloc(value_size);
    if (!new_key || !new_value) {
        free(new_key);
        free(new_value);
        free(new_node->forward);
        free(new_node);
        return PPDB_BASE_ERR_MEMORY;
    }

    memcpy(new_key, key, key_size);
    memcpy(new_value, value, value_size);

    new_node->key = new_key;
    new_node->key_size = key_size;
    new_node->value = new_value;
    new_node->value_size = value_size;
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
ppdb_error_t ppdb_base_skiplist_find(ppdb_base_skiplist_t* list, const void* key, size_t key_size, void** value, size_t* value_size) {
    if (!list || !key || !value || !value_size) return PPDB_BASE_ERR_PARAM;
    (void)key_size;  // Unused parameter

    ppdb_base_skiplist_node_t* current = list->head;

    // Search for key
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];

    // Check if key found
    if (current && list->compare(current->key, key) == 0) {
        *value = current->value;
        *value_size = current->value_size;
        return PPDB_OK;
    }

    return PPDB_BASE_ERR_NOT_FOUND;
}

// Remove from skiplist
ppdb_error_t ppdb_base_skiplist_remove(ppdb_base_skiplist_t* list, const void* key, size_t key_size) {
    if (!list || !key) return PPDB_BASE_ERR_PARAM;
    (void)key_size;  // Unused parameter

    ppdb_base_skiplist_node_t* update[PPDB_MAX_SKIPLIST_LEVEL];
    ppdb_base_skiplist_node_t* current = list->head;

    // Find node to remove
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];

    // Remove node if found
    if (current && list->compare(current->key, key) == 0) {
        for (int i = 0; i < list->max_level; i++) {
            if (update[i]->forward[i] != current) {
                break;
            }
            update[i]->forward[i] = current->forward[i];
        }

        // Update list level
        while (list->max_level > 1 && list->head->forward[list->max_level - 1] == NULL) {
            list->max_level--;
        }

        free(current->key);
        free(current->value);
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
ppdb_error_t ppdb_base_skiplist_clear(ppdb_base_skiplist_t* list) {
    if (!list) return PPDB_BASE_ERR_PARAM;

    ppdb_base_skiplist_node_t* current = list->head->forward[0];
    while (current) {
        ppdb_base_skiplist_node_t* next = current->forward[0];
        free(current->forward);
        free(current);
        current = next;
    }

    for (int i = 0; i < PPDB_MAX_SKIPLIST_LEVEL; i++) {
        list->head->forward[i] = NULL;
    }

    list->max_level = 1;
    list->size = 0;
    return PPDB_OK;
} 