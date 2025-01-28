/*
 * poly_ds.h - Data Structure Definitions at Poly Layer
 */

#ifndef POLY_DS_H
#define POLY_DS_H

#include "poly_core.h"

//-----------------------------------------------------------------------------
// List Operations
//-----------------------------------------------------------------------------

typedef struct poly_list_node {
    void* value;
    struct poly_list_node* next;
    struct poly_list_node* prev;
} poly_list_node_t;

typedef struct {
    poly_list_node_t* head;
    poly_list_node_t* tail;
    size_t size;
} poly_list_t;

poly_error_t poly_list_create(poly_list_t** list);
void poly_list_destroy(poly_list_t* list);
poly_error_t poly_list_append(poly_list_t* list, void* value);
poly_error_t poly_list_remove(poly_list_t* list, poly_list_node_t* node);
poly_list_node_t* poly_list_head(poly_list_t* list);
poly_list_node_t* poly_list_node_next(poly_list_node_t* node);
void* poly_list_node_value(poly_list_node_t* node);

//-----------------------------------------------------------------------------
// Hash Table Operations
//-----------------------------------------------------------------------------

typedef struct poly_hash_entry {
    char* key;
    void* value;
    struct poly_hash_entry* next;
} poly_hash_entry_t;

typedef struct {
    poly_hash_entry_t** buckets;
    size_t capacity;
    size_t size;
} poly_hash_t;

poly_error_t poly_hash_create(poly_hash_t** hash, size_t capacity);
void poly_hash_destroy(poly_hash_t* hash);
poly_error_t poly_hash_put(poly_hash_t* hash, const char* key, void* value);
void* poly_hash_get(poly_hash_t* hash, const char* key);
void poly_hash_remove(poly_hash_t* hash, const char* key);
void poly_hash_clear(poly_hash_t* hash);

//-----------------------------------------------------------------------------
// Red-Black Tree Operations
//-----------------------------------------------------------------------------

typedef enum {
    POLY_RBTREE_BLACK,
    POLY_RBTREE_RED
} poly_rbtree_color_t;

typedef struct poly_rbtree_node {
    int key;
    void* value;
    poly_rbtree_color_t color;
    struct poly_rbtree_node* parent;
    struct poly_rbtree_node* left;
    struct poly_rbtree_node* right;
} poly_rbtree_node_t;

typedef struct {
    poly_rbtree_node_t* root;
    poly_rbtree_node_t* nil;
    size_t size;
} poly_rbtree_t;

poly_error_t poly_rbtree_create(poly_rbtree_t** tree);
void poly_rbtree_destroy(poly_rbtree_t* tree);
poly_error_t poly_rbtree_insert(poly_rbtree_t* tree, int key, void* value);
void* poly_rbtree_find(poly_rbtree_t* tree, int key);
void poly_rbtree_remove(poly_rbtree_t* tree, int key);
void poly_rbtree_clear(poly_rbtree_t* tree);

#endif // POLY_DS_H
