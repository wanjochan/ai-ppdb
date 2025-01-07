/*
 * base_skiplist.inc.c - Skip List Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Internal functions
static int random_level(void) {
    int level = 1;
    while ((ppdb_base_rand() & 0xFFFF) < (0xFFFF >> 1) && level < PPDB_MAX_SKIPLIST_LEVEL) {
        level++;
    }
    return level;
}

static struct ppdb_base_skiplist_node_s* create_node(int level, const void* key, void* value) {
    struct ppdb_base_skiplist_node_s* node = (struct ppdb_base_skiplist_node_s*)malloc(
        sizeof(struct ppdb_base_skiplist_node_s) + level * sizeof(struct ppdb_base_skiplist_node_s*));
    if (!node) {
        return NULL;
    }

    node->key = key;
    node->value = value;
    node->level = level;
    node->forward = (struct ppdb_base_skiplist_node_s**)((char*)node + sizeof(struct ppdb_base_skiplist_node_s));
    memset(node->forward, 0, level * sizeof(struct ppdb_base_skiplist_node_s*));

    return node;
}

// Create skiplist
ppdb_error_t ppdb_base_skiplist_create(ppdb_base_skiplist_t** list, ppdb_base_compare_func_t compare) {
    if (!list || !compare) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_skiplist_t* new_list = (ppdb_base_skiplist_t*)malloc(sizeof(ppdb_base_skiplist_t));
    if (!new_list) {
        return PPDB_BASE_ERR_MEMORY;
    }

    new_list->header = create_node(PPDB_MAX_SKIPLIST_LEVEL, NULL, NULL);
    if (!new_list->header) {
        free(new_list);
        return PPDB_BASE_ERR_MEMORY;
    }

    new_list->level = 1;
    new_list->size = 0;
    new_list->compare = compare;

    *list = new_list;
    return PPDB_OK;
}

// Destroy skiplist
ppdb_error_t ppdb_base_skiplist_destroy(ppdb_base_skiplist_t* list) {
    if (!list) {
        return PPDB_BASE_ERR_PARAM;
    }

    struct ppdb_base_skiplist_node_s* current = list->header;
    while (current) {
        struct ppdb_base_skiplist_node_s* next = current->forward[0];
        free(current);
        current = next;
    }

    free(list);
    return PPDB_OK;
}

// Insert into skiplist
ppdb_error_t ppdb_base_skiplist_insert(ppdb_base_skiplist_t* list, const void* key, void* value) {
    if (!list || !key) {
        return PPDB_BASE_ERR_PARAM;
    }

    struct ppdb_base_skiplist_node_s* update[PPDB_MAX_SKIPLIST_LEVEL];
    struct ppdb_base_skiplist_node_s* current = list->header;

    // Find position to insert
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    // Update existing node
    if (current && list->compare(current->key, key) == 0) {
        current->value = value;
        return PPDB_OK;
    }

    // Insert new node
    int new_level = random_level();
    if (new_level > list->level) {
        for (int i = list->level; i < new_level; i++) {
            update[i] = list->header;
        }
        list->level = new_level;
    }

    current = create_node(new_level, key, value);
    if (!current) {
        return PPDB_BASE_ERR_MEMORY;
    }

    for (int i = 0; i < new_level; i++) {
        current->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = current;
    }

    list->size++;
    return PPDB_OK;
}

// Find in skiplist
ppdb_error_t ppdb_base_skiplist_find(ppdb_base_skiplist_t* list, const void* key, void** value) {
    if (!list || !key || !value) {
        return PPDB_BASE_ERR_PARAM;
    }

    struct ppdb_base_skiplist_node_s* current = list->header;

    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
    }

    current = current->forward[0];

    if (current && list->compare(current->key, key) == 0) {
        *value = current->value;
        return PPDB_OK;
    }

    return PPDB_BASE_ERR_NOT_FOUND;
}

// Remove from skiplist
ppdb_error_t ppdb_base_skiplist_remove(ppdb_base_skiplist_t* list, const void* key) {
    if (!list || !key) {
        return PPDB_BASE_ERR_PARAM;
    }

    struct ppdb_base_skiplist_node_s* update[PPDB_MAX_SKIPLIST_LEVEL];
    struct ppdb_base_skiplist_node_s* current = list->header;

    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    if (!current || list->compare(current->key, key) != 0) {
        return PPDB_BASE_ERR_NOT_FOUND;
    }

    for (int i = 0; i < list->level; i++) {
        if (update[i]->forward[i] != current) {
            break;
        }
        update[i]->forward[i] = current->forward[i];
    }

    free(current);

    while (list->level > 1 && list->header->forward[list->level - 1] == NULL) {
        list->level--;
    }

    list->size--;
    return PPDB_OK;
}

// Get skiplist size
size_t ppdb_base_skiplist_size(const ppdb_base_skiplist_t* list) {
    return list ? list->size : 0;
}

// Clear skiplist
void ppdb_base_skiplist_clear(ppdb_base_skiplist_t* list) {
    if (!list) {
        return;
    }

    struct ppdb_base_skiplist_node_s* current = list->header->forward[0];
    while (current) {
        struct ppdb_base_skiplist_node_s* next = current->forward[0];
        free(current);
        current = next;
    }

    memset(list->header->forward, 0, list->level * sizeof(struct ppdb_base_skiplist_node_s*));
    list->level = 1;
    list->size = 0;
} 