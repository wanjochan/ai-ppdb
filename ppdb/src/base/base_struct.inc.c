/*
 * base_struct.inc.c - Data Structure Implementation
 *
 * This file contains:
 * 1. List implementation
 * 2. Hash table implementation
 * 3. Skip list implementation
 * 4. Counter implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

//-----------------------------------------------------------------------------
// List Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_list_init(ppdb_base_list_t** list) {
    ppdb_base_list_t* new_list;

    if (!list) {
        return PPDB_BASE_ERR_PARAM;
    }

    new_list = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_list_t));
    if (!new_list) {
        return PPDB_BASE_ERR_MEMORY;
    }

    new_list->head = NULL;
    new_list->tail = NULL;
    new_list->size = 0;
    new_list->cleanup = NULL;

    *list = new_list;
    return PPDB_OK;
}

void ppdb_base_list_destroy(ppdb_base_list_t* list) {
    if (!list) return;

    ppdb_base_list_node_t* current = list->head;
    ppdb_base_list_node_t* next;

    while (current) {
        next = current->next;
        if (list->cleanup) {
            list->cleanup(current->data);
        }
        ppdb_base_aligned_free(current);
        current = next;
    }

    ppdb_base_aligned_free(list);
}

ppdb_error_t ppdb_base_list_add(ppdb_base_list_t* list, void* data) {
    ppdb_base_list_node_t* node;

    if (!list) {
        return PPDB_BASE_ERR_PARAM;
    }

    node = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_list_node_t));
    if (!node) {
        return PPDB_BASE_ERR_MEMORY;
    }

    node->data = data;
    node->next = NULL;

    if (!list->head) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }

    list->size++;
    return PPDB_OK;
}

void* ppdb_base_list_pop_front(ppdb_base_list_t* list) {
    if (!list || !list->head) {
        return NULL;
    }

    ppdb_base_list_node_t* node = list->head;
    void* data = node->data;

    list->head = node->next;
    if (!list->head) {
        list->tail = NULL;
    }

    list->size--;
    ppdb_base_aligned_free(node);
    return data;
}

size_t ppdb_base_list_size(ppdb_base_list_t* list) {
    if (!list) return 0;
    return list->size;
}

void ppdb_base_list_set_cleanup(ppdb_base_list_t* list, ppdb_base_cleanup_func_t cleanup) {
    if (!list) return;
    list->cleanup = cleanup;
}

//-----------------------------------------------------------------------------
// Hash Table Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_hash_init(ppdb_base_hash_t** hash, size_t bucket_count, ppdb_base_compare_func_t compare) {
    ppdb_base_hash_t* new_hash;

    if (!hash || bucket_count == 0 || !compare) {
        return PPDB_BASE_ERR_PARAM;
    }

    new_hash = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_hash_t));
    if (!new_hash) {
        return PPDB_BASE_ERR_MEMORY;
    }

    new_hash->buckets = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_hash_entry_t*) * bucket_count);
    if (!new_hash->buckets) {
        ppdb_base_aligned_free(new_hash);
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_hash->buckets, 0, sizeof(ppdb_base_hash_entry_t*) * bucket_count);
    new_hash->bucket_count = bucket_count;
    new_hash->size = 0;
    new_hash->compare = compare;
    new_hash->cleanup = NULL;

    *hash = new_hash;
    return PPDB_OK;
}

void ppdb_base_hash_destroy(ppdb_base_hash_t* hash) {
    if (!hash) return;

    for (size_t i = 0; i < hash->bucket_count; i++) {
        ppdb_base_hash_entry_t* entry = hash->buckets[i];
        while (entry) {
            ppdb_base_hash_entry_t* next = entry->next;
            if (hash->cleanup) {
                hash->cleanup(entry->value);
            }
            ppdb_base_aligned_free(entry);
            entry = next;
        }
    }

    ppdb_base_aligned_free(hash->buckets);
    ppdb_base_aligned_free(hash);
}

ppdb_error_t ppdb_base_hash_put(ppdb_base_hash_t* hash, void* key, void* value) {
    ppdb_base_hash_entry_t* entry;
    size_t bucket;

    if (!hash || !key) {
        return PPDB_BASE_ERR_PARAM;
    }

    bucket = ppdb_base_str_hash(key) % hash->bucket_count;
    entry = hash->buckets[bucket];

    while (entry) {
        if (hash->compare(entry->key, key) == 0) {
            if (hash->cleanup) {
                hash->cleanup(entry->value);
            }
            entry->value = value;
            return PPDB_OK;
        }
        entry = entry->next;
    }

    entry = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_hash_entry_t));
    if (!entry) {
        return PPDB_BASE_ERR_MEMORY;
    }

    entry->key = key;
    entry->value = value;
    entry->next = hash->buckets[bucket];
    hash->buckets[bucket] = entry;
    hash->size++;

    return PPDB_OK;
}

ppdb_error_t ppdb_base_hash_get(ppdb_base_hash_t* hash, const void* key, void** value) {
    ppdb_base_hash_entry_t* entry;
    size_t bucket;

    if (!hash || !key || !value) {
        return PPDB_BASE_ERR_PARAM;
    }

    bucket = ppdb_base_str_hash(key) % hash->bucket_count;
    entry = hash->buckets[bucket];

    while (entry) {
        if (hash->compare(entry->key, key) == 0) {
            *value = entry->value;
            return PPDB_OK;
        }
        entry = entry->next;
    }

    return PPDB_BASE_ERR_NOT_FOUND;
}

ppdb_error_t ppdb_base_hash_remove(ppdb_base_hash_t* hash, const void* key) {
    ppdb_base_hash_entry_t* entry;
    ppdb_base_hash_entry_t* prev = NULL;
    size_t bucket;

    if (!hash || !key) {
        return PPDB_BASE_ERR_PARAM;
    }

    bucket = ppdb_base_str_hash(key) % hash->bucket_count;
    entry = hash->buckets[bucket];

    while (entry) {
        if (hash->compare(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                hash->buckets[bucket] = entry->next;
            }
            if (hash->cleanup) {
                hash->cleanup(entry->value);
            }
            ppdb_base_aligned_free(entry);
            hash->size--;
            return PPDB_OK;
        }
        prev = entry;
        entry = entry->next;
    }

    return PPDB_BASE_ERR_NOT_FOUND;
}

size_t ppdb_base_hash_size(ppdb_base_hash_t* hash) {
    if (!hash) return 0;
    return hash->size;
}

void ppdb_base_hash_set_cleanup(ppdb_base_hash_t* hash, ppdb_base_cleanup_func_t cleanup) {
    if (!hash) return;
    hash->cleanup = cleanup;
}

//-----------------------------------------------------------------------------
// Skip List Implementation
//-----------------------------------------------------------------------------

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

ppdb_error_t ppdb_base_skiplist_iterator_destroy(ppdb_base_skiplist_iterator_t* it) {
    if (!it) return PPDB_BASE_ERR_PARAM;
    free(it);
    return PPDB_OK;
}

bool ppdb_base_skiplist_iterator_valid(ppdb_base_skiplist_iterator_t* it) {
    return it && it->current != NULL;
}

ppdb_error_t ppdb_base_skiplist_iterator_value(ppdb_base_skiplist_iterator_t* iterator, void** value, size_t* value_size) {
    if (!iterator || !value || !value_size) return PPDB_BASE_ERR_PARAM;
    if (!iterator->current) return PPDB_BASE_ERR_NOT_FOUND;
    
    *value = iterator->current->value;
    *value_size = iterator->current->value_size;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_skiplist_iterator_next(ppdb_base_skiplist_iterator_t* it) {
    if (!it) return PPDB_BASE_ERR_PARAM;
    if (it->current) {
        it->current = it->current->forward[0];
    }
    return PPDB_OK;
}

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

size_t ppdb_base_skiplist_size(const ppdb_base_skiplist_t* list) {
    if (!list) return 0;
    return list->size;
}

ppdb_error_t ppdb_base_skiplist_clear(ppdb_base_skiplist_t* list) {
    if (!list) return PPDB_BASE_ERR_PARAM;

    ppdb_base_skiplist_node_t* current = list->head->forward[0];
    while (current) {
        ppdb_base_skiplist_node_t* next = current->forward[0];
        free(current->key);
        free(current->value);
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

//-----------------------------------------------------------------------------
// Counter Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter, const char* name) {
    if (!counter || !name) return PPDB_BASE_ERR_PARAM;

    ppdb_base_counter_t* new_counter = (ppdb_base_counter_t*)malloc(sizeof(ppdb_base_counter_t));
    if (!new_counter) return PPDB_BASE_ERR_MEMORY;

    atomic_init(&new_counter->value, 0);
    new_counter->name = strdup(name);
    if (!new_counter->name) {
        free(new_counter);
        return PPDB_BASE_ERR_MEMORY;
    }
    new_counter->stats_enabled = false;

    *counter = new_counter;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_destroy(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    free(counter->name);
    free(counter);
    return PPDB_OK;
}

uint64_t ppdb_base_counter_get(ppdb_base_counter_t* counter) {
    if (!counter) return 0;
    return atomic_load(&counter->value);
}

ppdb_error_t ppdb_base_counter_set(ppdb_base_counter_t* counter, uint64_t value) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_store(&counter->value, value);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_increment(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_fetch_add(&counter->value, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_decrement(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_fetch_sub(&counter->value, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_add(ppdb_base_counter_t* counter, int64_t value) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_fetch_add(&counter->value, value);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_sub(ppdb_base_counter_t* counter, int64_t value) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_fetch_sub(&counter->value, value);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_compare_exchange(ppdb_base_counter_t* counter, int64_t expected, int64_t desired) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    uint64_t exp = expected;
    bool success = atomic_compare_exchange_strong(&counter->value, &exp, desired);
    return success ? PPDB_OK : PPDB_BASE_ERR_BUSY;
}

ppdb_error_t ppdb_base_counter_reset(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_store(&counter->value, 0);
    return PPDB_OK;
} 