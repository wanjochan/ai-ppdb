/*
 * base_struct.inc.c - Data Structure Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// List implementation
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

// Hash table implementation
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

    return PPDB_BASE_ERR_INVALID_STATE;
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

    return PPDB_BASE_ERR_INVALID_STATE;
}

size_t ppdb_base_hash_size(ppdb_base_hash_t* hash) {
    if (!hash) return 0;
    return hash->size;
}

void ppdb_base_hash_set_cleanup(ppdb_base_hash_t* hash, ppdb_base_cleanup_func_t cleanup) {
    if (!hash) return;
    hash->cleanup = cleanup;
}