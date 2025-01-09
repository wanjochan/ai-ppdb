/*
 * base_struct.inc.c - Data Structure Implementation
 *
 * This file contains:
 * 1. Linked list operations
 * 2. Hash table operations
 * 3. Skip list operations
 * 4. Counter operations
 */

#include <cosmopolitan.h>
#include "internal/base.h"

//-----------------------------------------------------------------------------
// List Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_list_init(ppdb_base_list_t* list) {
    if (!list) return PPDB_ERR_PARAM;
    
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    list->cleanup = NULL;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_destroy(ppdb_base_list_t* list) {
    if (!list) return PPDB_ERR_PARAM;

    ppdb_base_list_node_t* node = list->head;
    while (node) {
        ppdb_base_list_node_t* next = node->next;
        if (list->cleanup) {
            list->cleanup(node->data);
        }
        ppdb_base_mem_free(node);
        node = next;
    }

    memset(list, 0, sizeof(ppdb_base_list_t));
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_push_front(ppdb_base_list_t* list, void* data) {
    if (!list) return PPDB_ERR_PARAM;
    
    ppdb_base_list_node_t* node = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_list_node_t), (void**)&node);
    if (err != PPDB_OK) return err;
    
    node->data = data;
    node->next = list->head;
    node->prev = NULL;
    
    if (list->head) {
        list->head->prev = node;
    } else {
        list->tail = node;
    }
    
    list->head = node;
    list->size++;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_push_back(ppdb_base_list_t* list, void* data) {
    if (!list) return PPDB_ERR_PARAM;
    
    ppdb_base_list_node_t* node = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_list_node_t), (void**)&node);
    if (err != PPDB_OK) return err;
    
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

ppdb_error_t ppdb_base_list_pop_front(ppdb_base_list_t* list, void** out_data) {
    if (!list || !out_data) return PPDB_ERR_PARAM;
    if (!list->head) return PPDB_ERR_EMPTY;
    
    ppdb_base_list_node_t* node = list->head;
    *out_data = node->data;
    
    list->head = node->next;
    if (!list->head) list->tail = NULL;
    list->size--;
    
    ppdb_base_mem_free(node);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_pop_back(ppdb_base_list_t* list, void** out_data) {
    if (!list || !out_data) return PPDB_ERR_PARAM;
    if (!list->tail) return PPDB_ERR_EMPTY;
    
    ppdb_base_list_node_t* node = list->tail;
    *out_data = node->data;
    
    if (list->head == list->tail) {
        list->head = list->tail = NULL;
    } else {
        ppdb_base_list_node_t* prev = list->head;
        while (prev->next != list->tail) prev = prev->next;
        prev->next = NULL;
        list->tail = prev;
    }
    list->size--;
    
    ppdb_base_mem_free(node);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_front(ppdb_base_list_t* list, void** out_data) {
    if (!list || !out_data) {
        return PPDB_ERR_PARAM;
    }

    if (!list->head) {
        *out_data = NULL;
        return PPDB_ERR_EMPTY;
    }

    *out_data = list->head->data;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_back(ppdb_base_list_t* list, void** out_data) {
    if (!list || !out_data) {
        return PPDB_ERR_PARAM;
    }

    if (!list->tail) {
        *out_data = NULL;
        return PPDB_ERR_EMPTY;
    }

    *out_data = list->tail->data;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_size(ppdb_base_list_t* list, size_t* out_size) {
    if (!list || !out_size) {
        return PPDB_ERR_PARAM;
    }

    *out_size = list->size;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_empty(ppdb_base_list_t* list, bool* out_empty) {
    if (!list || !out_empty) {
        return PPDB_ERR_PARAM;
    }

    *out_empty = (list->size == 0);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_clear(ppdb_base_list_t* list) {
    if (!list) return PPDB_ERR_PARAM;
    
    ppdb_base_list_node_t* node = list->head;
    while (node) {
        ppdb_base_list_node_t* next = node->next;
        if (list->cleanup) list->cleanup(node->data);
        ppdb_base_mem_free(node);
        node = next;
    }
    
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_reverse(ppdb_base_list_t* list) {
    if (!list) return PPDB_ERR_PARAM;
    if (!list->head) return PPDB_OK;

    ppdb_base_list_node_t* curr = list->head;
    ppdb_base_list_node_t* temp = NULL;

    list->tail = list->head;

    while (curr) {
        temp = curr->prev;
        curr->prev = curr->next;
        curr->next = temp;
        curr = curr->prev;
    }

    if (temp) {
        list->head = temp->prev;
    }

    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Hash Table Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_hash_init(ppdb_base_hash_t* hash, size_t initial_size) {
    if (!hash) return PPDB_ERR_PARAM;
    if (initial_size == 0) initial_size = 16;  // Default size
    
    hash->buckets = NULL;
    hash->size = 0;
    hash->capacity = initial_size;
    hash->cleanup = NULL;
    
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_hash_node_t*) * initial_size, (void**)&hash->buckets);
    if (err != PPDB_OK) return err;
    
    memset(hash->buckets, 0, sizeof(ppdb_base_hash_node_t*) * initial_size);
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_hash_destroy(ppdb_base_hash_t* hash) {
    if (!hash) return PPDB_ERR_PARAM;
    
    for (size_t i = 0; i < hash->capacity; i++) {
        ppdb_base_hash_node_t* node = hash->buckets[i];
        while (node) {
            ppdb_base_hash_node_t* next = node->next;
            if (hash->cleanup) {
                hash->cleanup(node->value);
            }
            ppdb_base_mem_free(node);
            node = next;
        }
    }
    
    ppdb_base_mem_free(hash->buckets);
    hash->buckets = NULL;
    hash->size = 0;
    hash->capacity = 0;
    
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Skip List Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_skiplist_init(ppdb_base_skiplist_t* list, size_t max_level) {
    if (!list) return PPDB_ERR_PARAM;
    if (max_level > PPDB_MAX_SKIPLIST_LEVEL) max_level = PPDB_MAX_SKIPLIST_LEVEL;
    
    ppdb_base_skiplist_node_t* head = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_skiplist_node_t), (void**)&head);
    if (err != PPDB_OK) return err;
    
    head->key = NULL;
    head->value = NULL;
    head->key_size = 0;
    head->value_size = 0;
    head->level = max_level;
    
    err = ppdb_base_mem_malloc(sizeof(ppdb_base_skiplist_node_t*) * max_level, (void**)&head->forward);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(head);
        return err;
    }
    
    for (int i = 0; i < max_level; i++) {
        head->forward[i] = NULL;
    }
    
    list->head = head;
    list->level = 1;
    list->count = 0;
    list->cleanup = NULL;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_skiplist_destroy(ppdb_base_skiplist_t* list) {
    if (!list) return PPDB_ERR_PARAM;
    
    ppdb_base_skiplist_node_t* node = list->head;
    while (node) {
        ppdb_base_skiplist_node_t* next = node->forward[0];
        if (list->cleanup && node->value) {
            list->cleanup(node->value);
        }
        ppdb_base_mem_free(node->forward);
        ppdb_base_mem_free(node);
        node = next;
    }
    
    list->head = NULL;
    list->level = 0;
    list->count = 0;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_skiplist_size(ppdb_base_skiplist_t* list, size_t* out_size) {
    if (!list || !out_size) return PPDB_ERR_PARAM;
    
    *out_size = list->count;
    return PPDB_OK;
}

static int random_level(void) {
    int level = 1;
    while ((rand() & 0xFFFF) < (0xFFFF >> 1) && level < PPDB_MAX_SKIPLIST_LEVEL) {
        level++;
    }
    return level;
}

ppdb_error_t ppdb_base_skiplist_insert(ppdb_base_skiplist_t* list, const void* key, size_t key_size, const void* value, size_t value_size) {
    if (!list || !key || !value) return PPDB_ERR_PARAM;
    
    ppdb_base_skiplist_node_t* update[PPDB_MAX_SKIPLIST_LEVEL];
    ppdb_base_skiplist_node_t* current = list->head;
    
    // Find position to insert
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && memcmp(current->forward[i]->key, key, key_size) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];
    
    // Key exists, update value
    if (current && memcmp(current->key, key, key_size) == 0) {
        if (list->cleanup) {
            list->cleanup(current->value);
        }
        void* new_value;
        ppdb_error_t err = ppdb_base_mem_malloc(value_size, &new_value);
        if (err != PPDB_OK) return err;
        memcpy(new_value, value, value_size);
        current->value = new_value;
        current->value_size = value_size;
        return PPDB_OK;
    }
    
    // Create new node
    int level = random_level();
    if (level > list->level) {
        for (int i = list->level; i < level; i++) {
            update[i] = list->head;
        }
        list->level = level;
    }
    
    ppdb_base_skiplist_node_t* node;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_skiplist_node_t), (void**)&node);
    if (err != PPDB_OK) return err;
    
    node->level = level;
    err = ppdb_base_mem_malloc(sizeof(ppdb_base_skiplist_node_t*) * level, (void**)&node->forward);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(node);
        return err;
    }
    
    // Copy key and value
    void* new_key;
    err = ppdb_base_mem_malloc(key_size, &new_key);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(node->forward);
        ppdb_base_mem_free(node);
        return err;
    }
    memcpy(new_key, key, key_size);
    
    void* new_value;
    err = ppdb_base_mem_malloc(value_size, &new_value);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_key);
        ppdb_base_mem_free(node->forward);
        ppdb_base_mem_free(node);
        return err;
    }
    memcpy(new_value, value, value_size);
    
    node->key = new_key;
    node->key_size = key_size;
    node->value = new_value;
    node->value_size = value_size;
    
    // Update forward pointers
    for (int i = 0; i < level; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
    }
    
    list->count++;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_skiplist_find(ppdb_base_skiplist_t* list, const void* key, size_t key_size, void** value, size_t* value_size) {
    if (!list || !key || !value) return PPDB_ERR_PARAM;
    
    ppdb_base_skiplist_node_t* current = list->head;
    
    // Search from top level
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && memcmp(current->forward[i]->key, key, key_size) < 0) {
            current = current->forward[i];
        }
    }
    
    current = current->forward[0];
    if (current && memcmp(current->key, key, key_size) == 0) {
        *value = current->value;
        if (value_size) *value_size = current->value_size;
        return PPDB_OK;
    }
    
    return PPDB_ERR_NOT_FOUND;
}

//-----------------------------------------------------------------------------
// Counter Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter, const char* name) {
    if (!counter || !name) return PPDB_ERR_PARAM;

    void* counter_ptr;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_counter_t), &counter_ptr);
    if (err != PPDB_OK) {
        return err;
    }
    ppdb_base_counter_t* new_counter = (ppdb_base_counter_t*)counter_ptr;

    atomic_init(&new_counter->value, 0);
    new_counter->name = strdup(name);
    if (!new_counter->name) {
        ppdb_base_mem_free(new_counter);
        return PPDB_ERR_MEMORY;
    }
    new_counter->stats_enabled = false;

    *counter = new_counter;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_destroy(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_ERR_PARAM;
    free(counter->name);
    ppdb_base_mem_free(counter);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_get(ppdb_base_counter_t* counter, uint64_t* out_value) {
    if (!counter || !out_value) return PPDB_ERR_PARAM;
    *out_value = atomic_load(&counter->value);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_set(ppdb_base_counter_t* counter, uint64_t value) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_store(&counter->value, value);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_increment(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_fetch_add(&counter->value, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_decrement(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_fetch_sub(&counter->value, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_add(ppdb_base_counter_t* counter, int64_t value) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_fetch_add(&counter->value, value);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_sub(ppdb_base_counter_t* counter, int64_t value) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_fetch_sub(&counter->value, value);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_compare_exchange(ppdb_base_counter_t* counter, int64_t expected, int64_t desired) {
    if (!counter) return PPDB_ERR_PARAM;
    uint64_t exp = expected;
    bool success = atomic_compare_exchange_strong(&counter->value, &exp, desired);
    return success ? PPDB_OK : PPDB_ERR_BUSY;
}

ppdb_error_t ppdb_base_counter_reset(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_store(&counter->value, 0);
    return PPDB_OK;
} 