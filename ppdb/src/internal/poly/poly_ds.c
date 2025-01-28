/*
 * poly_ds.c - Core Data Structure Implementation at Poly Layer
 */

#include "poly_core.h"
#include "poly_memory.h"
#include "poly_ds.h"

//-----------------------------------------------------------------------------
// List Operations Implementation
//-----------------------------------------------------------------------------

poly_error_t poly_list_create(poly_list_t** list) {
    if (list == NULL) {
        return POLY_ERROR_INVALID_PARAM;
    }

    *list = (poly_list_t*)poly_malloc(sizeof(poly_list_t));
    if (*list == NULL) {
        return POLY_ERROR_NO_MEMORY;
    }

    (*list)->head = NULL;
    (*list)->tail = NULL;
    (*list)->size = 0;

    return POLY_OK;
}

void poly_list_destroy(poly_list_t* list) {
    if (list == NULL) {
        return;
    }

    poly_list_node_t* current = list->head;
    while (current != NULL) {
        poly_list_node_t* next = current->next;
        poly_free(current);
        current = next;
    }

    poly_free(list);
}

poly_error_t poly_list_append(poly_list_t* list, void* value) {
    if (list == NULL) {
        return POLY_ERROR_INVALID_PARAM;
    }

    poly_list_node_t* node = (poly_list_node_t*)poly_malloc(sizeof(poly_list_node_t));
    if (node == NULL) {
        return POLY_ERROR_NO_MEMORY;
    }

    node->value = value;
    node->next = NULL;
    node->prev = list->tail;

    if (list->tail != NULL) {
        list->tail->next = node;
    }
    list->tail = node;

    if (list->head == NULL) {
        list->head = node;
    }

    list->size++;
    return POLY_OK;
}

poly_error_t poly_list_remove(poly_list_t* list, poly_list_node_t* node) {
    if (list == NULL || node == NULL) {
        return POLY_ERROR_INVALID_PARAM;
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

    poly_free(node);
    list->size--;

    return POLY_OK;
}

poly_list_node_t* poly_list_head(poly_list_t* list) {
    return list ? list->head : NULL;
}

poly_list_node_t* poly_list_node_next(poly_list_node_t* node) {
    return node ? node->next : NULL;
}

void* poly_list_node_value(poly_list_node_t* node) {
    return node ? node->value : NULL;
}

//-----------------------------------------------------------------------------
// Hash Table Operations Implementation
//-----------------------------------------------------------------------------

static size_t hash_string(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

poly_error_t poly_hash_create(poly_hash_t** hash, size_t capacity) {
    if (hash == NULL || capacity == 0) {
        return POLY_ERROR_INVALID_PARAM;
    }

    *hash = (poly_hash_t*)poly_malloc(sizeof(poly_hash_t));
    if (*hash == NULL) {
        return POLY_ERROR_NO_MEMORY;
    }

    (*hash)->buckets = (poly_hash_entry_t**)poly_calloc(capacity, sizeof(poly_hash_entry_t*));
    if ((*hash)->buckets == NULL) {
        poly_free(*hash);
        return POLY_ERROR_NO_MEMORY;
    }

    (*hash)->capacity = capacity;
    (*hash)->size = 0;

    return POLY_OK;
}

void poly_hash_destroy(poly_hash_t* hash) {
    if (hash == NULL) {
        return;
    }

    for (size_t i = 0; i < hash->capacity; i++) {
        poly_hash_entry_t* entry = hash->buckets[i];
        while (entry != NULL) {
            poly_hash_entry_t* next = entry->next;
            poly_free(entry->key);
            poly_free(entry);
            entry = next;
        }
    }

    poly_free(hash->buckets);
    poly_free(hash);
}

poly_error_t poly_hash_put(poly_hash_t* hash, const char* key, void* value) {
    if (hash == NULL || key == NULL) {
        return POLY_ERROR_INVALID_PARAM;
    }

    size_t index = hash_string(key) % hash->capacity;
    poly_hash_entry_t* entry = hash->buckets[index];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return POLY_OK;
        }
        entry = entry->next;
    }

    entry = (poly_hash_entry_t*)poly_malloc(sizeof(poly_hash_entry_t));
    if (entry == NULL) {
        return POLY_ERROR_NO_MEMORY;
    }

    entry->key = poly_strdup(key);
    if (entry->key == NULL) {
        poly_free(entry);
        return POLY_ERROR_NO_MEMORY;
    }

    entry->value = value;
    entry->next = hash->buckets[index];
    hash->buckets[index] = entry;
    hash->size++;

    return POLY_OK;
}

void* poly_hash_get(poly_hash_t* hash, const char* key) {
    if (hash == NULL || key == NULL) {
        return NULL;
    }

    size_t index = hash_string(key) % hash->capacity;
    poly_hash_entry_t* entry = hash->buckets[index];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

void poly_hash_remove(poly_hash_t* hash, const char* key) {
    if (hash == NULL || key == NULL) {
        return;
    }

    size_t index = hash_string(key) % hash->capacity;
    poly_hash_entry_t* entry = hash->buckets[index];
    poly_hash_entry_t* prev = NULL;

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            if (prev == NULL) {
                hash->buckets[index] = entry->next;
            } else {
                prev->next = entry->next;
            }
            poly_free(entry->key);
            poly_free(entry);
            hash->size--;
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

void poly_hash_clear(poly_hash_t* hash) {
    if (hash == NULL) {
        return;
    }

    for (size_t i = 0; i < hash->capacity; i++) {
        poly_hash_entry_t* entry = hash->buckets[i];
        while (entry != NULL) {
            poly_hash_entry_t* next = entry->next;
            poly_free(entry->key);
            poly_free(entry);
            entry = next;
        }
        hash->buckets[i] = NULL;
    }
    hash->size = 0;
}

//-----------------------------------------------------------------------------
// Red-Black Tree Operations Implementation
//-----------------------------------------------------------------------------

static void transplant(poly_rbtree_t* tree, poly_rbtree_node_t* u, poly_rbtree_node_t* v) {
    if (u->parent == tree->nil) {
        tree->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

static poly_rbtree_node_t* minimum(poly_rbtree_node_t* node, poly_rbtree_node_t* nil) {
    while (node->left != nil) {
        node = node->left;
    }
    return node;
}

static void clear_node(poly_rbtree_t* tree, poly_rbtree_node_t* node) {
    if (node != tree->nil) {
        clear_node(tree, node->left);
        clear_node(tree, node->right);
        poly_free(node);
    }
}

static poly_rbtree_node_t* create_node(int key, void* value, poly_rbtree_node_t* nil) {
    poly_rbtree_node_t* node = (poly_rbtree_node_t*)poly_malloc(sizeof(poly_rbtree_node_t));
    if (node != NULL) {
        node->key = key;
        node->value = value;
        node->color = POLY_RBTREE_RED;
        node->left = nil;
        node->right = nil;
        node->parent = nil;
    }
    return node;
}

static void rotate_left(poly_rbtree_t* tree, poly_rbtree_node_t* x) {
    poly_rbtree_node_t* y = x->right;
    x->right = y->left;
    
    if (y->left != tree->nil) {
        y->left->parent = x;
    }
    
    y->parent = x->parent;
    
    if (x->parent == tree->nil) {
        tree->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    
    y->left = x;
    x->parent = y;
}

static void rotate_right(poly_rbtree_t* tree, poly_rbtree_node_t* y) {
    poly_rbtree_node_t* x = y->left;
    y->left = x->right;
    
    if (x->right != tree->nil) {
        x->right->parent = y;
    }
    
    x->parent = y->parent;
    
    if (y->parent == tree->nil) {
        tree->root = x;
    } else if (y == y->parent->right) {
        y->parent->right = x;
    } else {
        y->parent->left = x;
    }
    
    x->right = y;
    y->parent = x;
}

static void fix_insert(poly_rbtree_t* tree, poly_rbtree_node_t* z) {
    while (z->parent->color == POLY_RBTREE_RED) {
        if (z->parent == z->parent->parent->left) {
            poly_rbtree_node_t* y = z->parent->parent->right;
            if (y->color == POLY_RBTREE_RED) {
                z->parent->color = POLY_RBTREE_BLACK;
                y->color = POLY_RBTREE_BLACK;
                z->parent->parent->color = POLY_RBTREE_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    rotate_left(tree, z);
                }
                z->parent->color = POLY_RBTREE_BLACK;
                z->parent->parent->color = POLY_RBTREE_RED;
                rotate_right(tree, z->parent->parent);
            }
        } else {
            poly_rbtree_node_t* y = z->parent->parent->left;
            if (y->color == POLY_RBTREE_RED) {
                z->parent->color = POLY_RBTREE_BLACK;
                y->color = POLY_RBTREE_BLACK;
                z->parent->parent->color = POLY_RBTREE_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rotate_right(tree, z);
                }
                z->parent->color = POLY_RBTREE_BLACK;
                z->parent->parent->color = POLY_RBTREE_RED;
                rotate_left(tree, z->parent->parent);
            }
        }
    }
    tree->root->color = POLY_RBTREE_BLACK;
}

static void fix_delete(poly_rbtree_t* tree, poly_rbtree_node_t* x) {
    while (x != tree->root && x->color == POLY_RBTREE_BLACK) {
        if (x == x->parent->left) {
            poly_rbtree_node_t* w = x->parent->right;
            if (w->color == POLY_RBTREE_RED) {
                w->color = POLY_RBTREE_BLACK;
                x->parent->color = POLY_RBTREE_RED;
                rotate_left(tree, x->parent);
                w = x->parent->right;
            }
            if (w->left->color == POLY_RBTREE_BLACK && 
                w->right->color == POLY_RBTREE_BLACK) {
                w->color = POLY_RBTREE_RED;
                x = x->parent;
            } else {
                if (w->right->color == POLY_RBTREE_BLACK) {
                    w->left->color = POLY_RBTREE_BLACK;
                    w->color = POLY_RBTREE_RED;
                    rotate_right(tree, w);
                    w = x->parent->right;
                }
                w->color = x->parent->color;
                x->parent->color = POLY_RBTREE_BLACK;
                w->right->color = POLY_RBTREE_BLACK;
                rotate_left(tree, x->parent);
                x = tree->root;
            }
        } else {
            poly_rbtree_node_t* w = x->parent->left;
            if (w->color == POLY_RBTREE_RED) {
                w->color = POLY_RBTREE_BLACK;
                x->parent->color = POLY_RBTREE_RED;
                rotate_right(tree, x->parent);
                w = x->parent->left;
            }
            if (w->right->color == POLY_RBTREE_BLACK && 
                w->left->color == POLY_RBTREE_BLACK) {
                w->color = POLY_RBTREE_RED;
                x = x->parent;
            } else {
                if (w->left->color == POLY_RBTREE_BLACK) {
                    w->right->color = POLY_RBTREE_BLACK;
                    w->color = POLY_RBTREE_RED;
                    rotate_left(tree, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = POLY_RBTREE_BLACK;
                w->left->color = POLY_RBTREE_BLACK;
                rotate_right(tree, x->parent);
                x = tree->root;
            }
        }
    }
    x->color = POLY_RBTREE_BLACK;
}

poly_error_t poly_rbtree_create(poly_rbtree_t** tree) {
    if (tree == NULL) {
        return POLY_ERROR_INVALID_PARAM;
    }

    *tree = (poly_rbtree_t*)poly_malloc(sizeof(poly_rbtree_t));
    if (*tree == NULL) {
        return POLY_ERROR_NO_MEMORY;
    }

    (*tree)->nil = (poly_rbtree_node_t*)poly_malloc(sizeof(poly_rbtree_node_t));
    if ((*tree)->nil == NULL) {
        poly_free(*tree);
        return POLY_ERROR_NO_MEMORY;
    }

    (*tree)->nil->color = POLY_RBTREE_BLACK;
    (*tree)->root = (*tree)->nil;
    (*tree)->size = 0;

    return POLY_OK;
}

poly_error_t poly_rbtree_insert(poly_rbtree_t* tree, int key, void* value) {
    if (tree == NULL) {
        return POLY_ERROR_INVALID_PARAM;
    }

    poly_rbtree_node_t* z = create_node(key, value, tree->nil);
    if (z == NULL) {
        return POLY_ERROR_NO_MEMORY;
    }

    poly_rbtree_node_t* y = tree->nil;
    poly_rbtree_node_t* x = tree->root;

    while (x != tree->nil) {
        y = x;
        if (z->key < x->key) {
            x = x->left;
        } else if (z->key > x->key) {
            x = x->right;
        } else {
            x->value = value;
            poly_free(z);
            return POLY_OK;
        }
    }

    z->parent = y;
    if (y == tree->nil) {
        tree->root = z;
    } else if (z->key < y->key) {
        y->left = z;
    } else {
        y->right = z;
    }

    fix_insert(tree, z);
    tree->size++;

    return POLY_OK;
}

void* poly_rbtree_find(poly_rbtree_t* tree, int key) {
    if (tree == NULL) {
        return NULL;
    }

    poly_rbtree_node_t* x = tree->root;
    while (x != tree->nil) {
        if (key < x->key) {
            x = x->left;
        } else if (key > x->key) {
            x = x->right;
        } else {
            return x->value;
        }
    }

    return NULL;
}

void poly_rbtree_remove(poly_rbtree_t* tree, int key) {
    if (tree == NULL) {
        return;
    }

    poly_rbtree_node_t* z = tree->root;
    while (z != tree->nil) {
        if (key < z->key) {
            z = z->left;
        } else if (key > z->key) {
            z = z->right;
        } else {
            break;
        }
    }

    if (z == tree->nil) {
        return;
    }

    poly_rbtree_node_t* y = z;
    poly_rbtree_node_t* x;
    poly_rbtree_color_t y_original_color = y->color;

    if (z->left == tree->nil) {
        x = z->right;
        transplant(tree, z, z->right);
    } else if (z->right == tree->nil) {
        x = z->left;
        transplant(tree, z, z->left);
    } else {
        y = minimum(z->right, tree->nil);
        y_original_color = y->color;
        x = y->right;

        if (y->parent == z) {
            x->parent = y;
        } else {
            transplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }

        transplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }

    if (y_original_color == POLY_RBTREE_BLACK) {
        fix_delete(tree, x);
    }

    poly_free(z);
    tree->size--;
}

void poly_rbtree_clear(poly_rbtree_t* tree) {
    if (tree == NULL) {
        return;
    }
    clear_node(tree, tree->root);
    tree->root = tree->nil;
    tree->size = 0;
}

void poly_rbtree_destroy(poly_rbtree_t* tree) {
    if (tree == NULL) {
        return;
    }
    clear_node(tree, tree->root);
    poly_free(tree->nil);
    poly_free(tree);
}
