#ifndef PPDB_SKIPLIST_LOCKFREE_H
#define PPDB_SKIPLIST_LOCKFREE_H

#include <cosmopolitan.h>

#define MAX_LEVEL 32

// Node states
typedef enum {
    NODE_VALID = 0,
    NODE_DELETED = 1
} node_state_t;

// Lock-free skip list node structure
typedef struct skiplist_node_t {
    uint8_t* key;
    size_t key_len;
    void* value;
    size_t value_len;
    uint32_t level;
    atomic_uint state;
    struct ref_count_t* ref_count;
    _Atomic(struct skiplist_node_t*) next[];
} skiplist_node_t;

// Lock-free skip list structure
typedef struct atomic_skiplist_t {
    skiplist_node_t* head;
    atomic_size_t size;
    uint32_t max_level;
} atomic_skiplist_t;

// Visitor callback function type for traversal
typedef bool (*skiplist_visitor_t)(const uint8_t* key, size_t key_len,
                                 const uint8_t* value, size_t value_len,
                                 void* ctx);

// Basic operations
atomic_skiplist_t* atomic_skiplist_create(void);
void atomic_skiplist_destroy(atomic_skiplist_t* list);

int atomic_skiplist_put(atomic_skiplist_t* list,
                       const uint8_t* key, size_t key_len,
                       const uint8_t* value, size_t value_len);

int atomic_skiplist_get(atomic_skiplist_t* list,
                       const uint8_t* key, size_t key_len,
                       uint8_t* value, size_t* value_len);

int atomic_skiplist_delete(atomic_skiplist_t* list,
                         const uint8_t* key, size_t key_len);

size_t atomic_skiplist_size(atomic_skiplist_t* list);
void atomic_skiplist_clear(atomic_skiplist_t* list);

// Traverse the skip list with visitor pattern
void atomic_skiplist_foreach(atomic_skiplist_t* list,
                           skiplist_visitor_t visitor,
                           void* ctx);

#endif // PPDB_SKIPLIST_LOCKFREE_H 