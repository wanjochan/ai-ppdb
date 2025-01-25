#ifndef POLY_HASHTABLE_H
#define POLY_HASHTABLE_H

#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// Forward declaration of hashtable structure
typedef struct poly_hashtable poly_hashtable_t;

// Hash function type
typedef size_t (*poly_hash_fn)(const void* key);

// Key comparison function type
typedef bool (*poly_key_compare_fn)(const void* key1, const void* key2);

// Key-value pair
typedef struct {
    void* key;
    void* value;
} poly_hashtable_entry_t;

// Iterator callback function type
typedef void (*poly_hashtable_iter_fn)(const poly_hashtable_entry_t* entry, void* user_data);

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// Create a new hashtable
infra_error_t poly_hashtable_create(
    size_t initial_size,
    poly_hash_fn hash_fn,
    poly_key_compare_fn key_compare_fn,
    poly_hashtable_t** out_hashtable
);

// Destroy hashtable and free all resources
void poly_hashtable_destroy(poly_hashtable_t* hashtable);

// Insert or update a key-value pair
infra_error_t poly_hashtable_put(
    poly_hashtable_t* hashtable,
    void* key,
    void* value
);

// Get value by key
infra_error_t poly_hashtable_get(
    const poly_hashtable_t* hashtable,
    const void* key,
    void** out_value
);

// Remove key-value pair
infra_error_t poly_hashtable_remove(
    poly_hashtable_t* hashtable,
    const void* key
);

// Iterate over all entries
void poly_hashtable_foreach(
    poly_hashtable_t* hashtable,
    poly_hashtable_iter_fn iter_fn,
    void* user_data
);

// Get current number of entries
size_t poly_hashtable_size(const poly_hashtable_t* hashtable);

// Get current capacity
size_t poly_hashtable_capacity(const poly_hashtable_t* hashtable);

// Clear all entries
void poly_hashtable_clear(poly_hashtable_t* hashtable);

// Check if hashtable is being iterated
bool poly_hashtable_is_iterating(const poly_hashtable_t* hashtable);

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

// String hash function
size_t poly_hashtable_string_hash(const void* key);

// String comparison function
bool poly_hashtable_string_compare(const void* key1, const void* key2);

#endif // POLY_HASHTABLE_H
