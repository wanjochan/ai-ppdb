#ifndef INFRA_STRUCT_H
#define INFRA_STRUCT_H

#include "cosmopolitan.h"
#include "internal/infra/infra_error.h"

// List node structure
typedef struct infra_list_node {
    struct infra_list_node* next;
    void* value;
} infra_list_node_t;

// List structure
typedef struct infra_list {
    infra_list_node_t* head;
    infra_list_node_t* tail;
    size_t size;
} infra_list_t;

// Hash entry structure
typedef struct infra_hash_entry {
    char* key;
    void* value;
    struct infra_hash_entry* next;
} infra_hash_entry_t;

// Hash table structure
typedef struct infra_hash {
    infra_hash_entry_t** buckets;
    size_t capacity;
    size_t size;
} infra_hash_t;

// Red-black tree node color
typedef enum {
    INFRA_RBTREE_RED,
    INFRA_RBTREE_BLACK
} infra_rbtree_color_t;

// Red-black tree node structure
typedef struct infra_rbtree_node {
    int key;
    void* value;
    infra_rbtree_color_t color;
    struct infra_rbtree_node* parent;
    struct infra_rbtree_node* left;
    struct infra_rbtree_node* right;
} infra_rbtree_node_t;

// Red-black tree structure
typedef struct infra_rbtree {
    infra_rbtree_node_t* root;
    size_t size;
} infra_rbtree_t;

// List operations
infra_error_t infra_list_create(infra_list_t** list);
void infra_list_destroy(infra_list_t* list);
infra_error_t infra_list_append(infra_list_t* list, void* value);
infra_error_t infra_list_remove(infra_list_t* list, infra_list_node_t* node);
infra_list_node_t* infra_list_head(infra_list_t* list);
infra_list_node_t* infra_list_node_next(infra_list_node_t* node);
void* infra_list_node_value(infra_list_node_t* node);

// Hash table operations
infra_error_t infra_hash_create(infra_hash_t** hash, size_t capacity);
void infra_hash_destroy(infra_hash_t* hash);
infra_error_t infra_hash_put(infra_hash_t* hash, const char* key, void* value);
void* infra_hash_get(infra_hash_t* hash, const char* key);
void* infra_hash_remove(infra_hash_t* hash, const char* key);
void infra_hash_clear(infra_hash_t* hash);

// Red-black tree operations
infra_error_t infra_rbtree_create(infra_rbtree_t** tree);
void infra_rbtree_destroy(infra_rbtree_t* tree);
infra_error_t infra_rbtree_insert(infra_rbtree_t* tree, int key, void* value);
void* infra_rbtree_find(infra_rbtree_t* tree, int key);
void* infra_rbtree_remove(infra_rbtree_t* tree, int key);
void infra_rbtree_clear(infra_rbtree_t* tree);

#endif /* INFRA_STRUCT_H */ 