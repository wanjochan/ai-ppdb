#include "internal/infra/infra_core.h"
#include "internal/poly/poly_hashtable.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define POLY_HASHTABLE_MIN_SIZE 16
#define POLY_HASHTABLE_LOAD_FACTOR 0.75
#define POLY_HASHTABLE_GROWTH_FACTOR 2

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// Hash table entry node
typedef struct poly_hashtable_node {
    poly_hashtable_entry_t entry;
    struct poly_hashtable_node* next;
} poly_hashtable_node_t;

// Hash table structure
struct poly_hashtable {
    poly_hashtable_node_t** buckets;
    size_t bucket_count;
    size_t size;
    double load_factor;
    poly_hash_fn hash_fn;
    poly_key_compare_fn key_compare_fn;
};

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static size_t next_power_of_2(size_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;
    return n;
}

static infra_error_t resize_hashtable(poly_hashtable_t* hashtable, size_t new_size) {
    poly_hashtable_node_t** new_buckets = calloc(new_size, sizeof(poly_hashtable_node_t*));
    if (!new_buckets) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // Rehash all entries
    for (size_t i = 0; i < hashtable->bucket_count; i++) {
        poly_hashtable_node_t* node = hashtable->buckets[i];
        while (node) {
            poly_hashtable_node_t* next = node->next;
            size_t new_index = hashtable->hash_fn(node->entry.key) % new_size;
            node->next = new_buckets[new_index];
            new_buckets[new_index] = node;
            node = next;
        }
    }

    free(hashtable->buckets);
    hashtable->buckets = new_buckets;
    hashtable->bucket_count = new_size;
    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Implementation
//-----------------------------------------------------------------------------

infra_error_t poly_hashtable_create(
    size_t initial_size,
    poly_hash_fn hash_fn,
    poly_key_compare_fn key_compare_fn,
    poly_hashtable_t** out_hashtable
) {
    if (!hash_fn || !key_compare_fn || !out_hashtable) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_hashtable_t* hashtable = malloc(sizeof(poly_hashtable_t));
    if (!hashtable) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // Ensure minimum size and power of 2
    initial_size = next_power_of_2(
        initial_size < POLY_HASHTABLE_MIN_SIZE ? 
        POLY_HASHTABLE_MIN_SIZE : initial_size
    );

    hashtable->buckets = calloc(initial_size, sizeof(poly_hashtable_node_t*));
    if (!hashtable->buckets) {
        free(hashtable);
        return INFRA_ERROR_NO_MEMORY;
    }

    hashtable->bucket_count = initial_size;
    hashtable->size = 0;
    hashtable->load_factor = POLY_HASHTABLE_LOAD_FACTOR;
    hashtable->hash_fn = hash_fn;
    hashtable->key_compare_fn = key_compare_fn;

    *out_hashtable = hashtable;
    return INFRA_OK;
}

void poly_hashtable_destroy(poly_hashtable_t* hashtable) {
    if (!hashtable) {
        return;
    }

    // Free all nodes
    for (size_t i = 0; i < hashtable->bucket_count; i++) {
        poly_hashtable_node_t* node = hashtable->buckets[i];
        while (node) {
            poly_hashtable_node_t* next = node->next;
            free(node);
            node = next;
        }
    }

    free(hashtable->buckets);
    free(hashtable);
}

infra_error_t poly_hashtable_put(
    poly_hashtable_t* hashtable,
    void* key,
    void* value
) {
    if (!hashtable || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Check if resize is needed
    if ((double)(hashtable->size + 1) / hashtable->bucket_count > hashtable->load_factor) {
        infra_error_t err = resize_hashtable(hashtable, hashtable->bucket_count * POLY_HASHTABLE_GROWTH_FACTOR);
        if (err != INFRA_OK) {
            return err;
        }
    }

    size_t index = hashtable->hash_fn(key) % hashtable->bucket_count;
    poly_hashtable_node_t* node = hashtable->buckets[index];

    // Check for existing key
    while (node) {
        if (hashtable->key_compare_fn(node->entry.key, key)) {
            node->entry.value = value;
            return INFRA_OK;
        }
        node = node->next;
    }

    // Create new node
    poly_hashtable_node_t* new_node = malloc(sizeof(poly_hashtable_node_t));
    if (!new_node) {
        return INFRA_ERROR_NO_MEMORY;
    }

    new_node->entry.key = key;
    new_node->entry.value = value;
    new_node->next = hashtable->buckets[index];
    hashtable->buckets[index] = new_node;
    hashtable->size++;

    return INFRA_OK;
}

infra_error_t poly_hashtable_get(
    const poly_hashtable_t* hashtable,
    const void* key,
    void** out_value
) {
    if (!hashtable || !key || !out_value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t index = hashtable->hash_fn(key) % hashtable->bucket_count;
    poly_hashtable_node_t* node = hashtable->buckets[index];

    while (node) {
        if (hashtable->key_compare_fn(node->entry.key, key)) {
            *out_value = node->entry.value;
            return INFRA_OK;
        }
        node = node->next;
    }

    return INFRA_ERROR_NOT_FOUND;
}

infra_error_t poly_hashtable_remove(
    poly_hashtable_t* hashtable,
    const void* key
) {
    if (!hashtable || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t index = hashtable->hash_fn(key) % hashtable->bucket_count;
    poly_hashtable_node_t* node = hashtable->buckets[index];
    poly_hashtable_node_t* prev = NULL;

    while (node) {
        if (hashtable->key_compare_fn(node->entry.key, key)) {
            if (prev) {
                prev->next = node->next;
            } else {
                hashtable->buckets[index] = node->next;
            }
            free(node);
            hashtable->size--;
            return INFRA_OK;
        }
        prev = node;
        node = node->next;
    }

    return INFRA_ERROR_NOT_FOUND;
}

void poly_hashtable_foreach(
    const poly_hashtable_t* hashtable,
    poly_hashtable_iter_fn iter_fn,
    void* user_data
) {
    if (!hashtable || !iter_fn) {
        return;
    }

    for (size_t i = 0; i < hashtable->bucket_count; i++) {
        poly_hashtable_node_t* node = hashtable->buckets[i];
        while (node) {
            iter_fn(&node->entry, user_data);
            node = node->next;
        }
    }
}

size_t poly_hashtable_size(const poly_hashtable_t* hashtable) {
    return hashtable ? hashtable->size : 0;
}

size_t poly_hashtable_capacity(const poly_hashtable_t* hashtable) {
    return hashtable ? hashtable->bucket_count : 0;
}

void poly_hashtable_clear(poly_hashtable_t* hashtable) {
    if (!hashtable) {
        return;
    }

    for (size_t i = 0; i < hashtable->bucket_count; i++) {
        poly_hashtable_node_t* node = hashtable->buckets[i];
        while (node) {
            poly_hashtable_node_t* next = node->next;
            free(node);
            node = next;
        }
        hashtable->buckets[i] = NULL;
    }
    hashtable->size = 0;
}

//-----------------------------------------------------------------------------
// Utility Functions Implementation
//-----------------------------------------------------------------------------

size_t poly_hashtable_string_hash(const void* key) {
    const char* str = (const char*)key;
    size_t hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }

    return hash;
}

bool poly_hashtable_string_compare(const void* key1, const void* key2) {
    return strcmp((const char*)key1, (const char*)key2) == 0;
}
