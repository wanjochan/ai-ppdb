/*
 * infra_ds.c - Core Data Structure Implementation
 */

#include "infra_core.h"
#include "infra_memory.h"
#include "infra_ds.h"

//-----------------------------------------------------------------------------
// List Operations Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_list_create(infra_list_t** list) {
    if (list == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *list = (infra_list_t*)infra_malloc(sizeof(infra_list_t));
    if (*list == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }

    (*list)->head = NULL;
    (*list)->tail = NULL;
    (*list)->size = 0;

    return INFRA_OK;
}

void infra_list_destroy(infra_list_t* list) {
    if (list == NULL) {
        return;
    }

    infra_list_node_t* current = list->head;
    while (current != NULL) {
        infra_list_node_t* next = current->next;
        infra_free(current);
        current = next;
    }

    infra_free(list);
}

infra_error_t infra_list_append(infra_list_t* list, void* value) {
    if (list == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_list_node_t* node = (infra_list_node_t*)infra_malloc(sizeof(infra_list_node_t));
    if (node == NULL) {
        return INFRA_ERROR_NO_MEMORY;
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
    return INFRA_OK;
}

infra_error_t infra_list_remove(infra_list_t* list, infra_list_node_t* node) {
    if (list == NULL || node == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
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

    infra_free(node);
    list->size--;

    return INFRA_OK;
}

infra_list_node_t* infra_list_head(infra_list_t* list) {
    return list ? list->head : NULL;
}

infra_list_node_t* infra_list_node_next(infra_list_node_t* node) {
    return node ? node->next : NULL;
}

void* infra_list_node_value(infra_list_node_t* node) {
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

infra_error_t infra_hash_create(infra_hash_t** hash, size_t capacity) {
    if (hash == NULL || capacity == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *hash = (infra_hash_t*)infra_malloc(sizeof(infra_hash_t));
    if (*hash == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }

    (*hash)->buckets = (infra_hash_node_t**)infra_malloc(sizeof(infra_hash_node_t*) * capacity);
    if ((*hash)->buckets == NULL) {
        infra_free(*hash);
        *hash = NULL;
        return INFRA_ERROR_NO_MEMORY;
    }

    for (size_t i = 0; i < capacity; i++) {
        (*hash)->buckets[i] = NULL;
    }

    (*hash)->capacity = capacity;
    (*hash)->size = 0;

    return INFRA_OK;
}

void infra_hash_destroy(infra_hash_t* hash) {
    if (hash == NULL) {
        return;
    }

    for (size_t i = 0; i < hash->capacity; i++) {
        infra_hash_node_t* entry = hash->buckets[i];
        while (entry != NULL) {
            infra_hash_node_t* next = entry->next;
            infra_free(entry->key);
            infra_free(entry);
            entry = next;
        }
    }

    infra_free(hash->buckets);
    infra_free(hash);
}

infra_error_t infra_hash_put(infra_hash_t* hash, const char* key, void* value) {
    if (hash == NULL || key == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t index = hash_string(key) % hash->capacity;
    infra_hash_node_t* entry = hash->buckets[index];

    // 查找是否已存在
    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return INFRA_OK;
        }
        entry = entry->next;
    }

    // 创建新条目
    entry = (infra_hash_node_t*)infra_malloc(sizeof(infra_hash_node_t));
    if (entry == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }

    entry->key = infra_strdup(key);
    if (entry->key == NULL) {
        infra_free(entry);
        return INFRA_ERROR_NO_MEMORY;
    }

    entry->value = value;
    entry->next = hash->buckets[index];
    hash->buckets[index] = entry;
    hash->size++;

    return INFRA_OK;
}

void* infra_hash_get(infra_hash_t* hash, const char* key) {
    if (hash == NULL || key == NULL) {
        return NULL;
    }

    size_t index = hash_string(key) % hash->capacity;
    infra_hash_node_t* entry = hash->buckets[index];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

void* infra_hash_remove(infra_hash_t* hash, const char* key) {
    if (hash == NULL || key == NULL) {
        return NULL;
    }

    size_t index = hash_string(key) % hash->capacity;
    infra_hash_node_t* entry = hash->buckets[index];
    infra_hash_node_t* prev = NULL;

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            void* value = entry->value;
            if (prev == NULL) {
                hash->buckets[index] = entry->next;
            } else {
                prev->next = entry->next;
            }
            infra_free(entry->key);
            infra_free(entry);
            hash->size--;
            return value;
        }
        prev = entry;
        entry = entry->next;
    }

    return NULL;
}

void infra_hash_clear(infra_hash_t* hash) {
    if (hash == NULL) {
        return;
    }

    for (size_t i = 0; i < hash->capacity; i++) {
        infra_hash_node_t* entry = hash->buckets[i];
        while (entry != NULL) {
            infra_hash_node_t* next = entry->next;
            infra_free(entry->key);
            infra_free(entry);
            entry = next;
        }
        hash->buckets[i] = NULL;
    }
    hash->size = 0;
}

//-----------------------------------------------------------------------------
// Red-Black Tree Operations Implementation
//-----------------------------------------------------------------------------

static void transplant(infra_rbtree_t* tree, infra_rbtree_node_t* u, infra_rbtree_node_t* v) {
    if (u->parent == tree->nil) {
        tree->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

static infra_rbtree_node_t* minimum(infra_rbtree_t* tree, infra_rbtree_node_t* node) {
    while (node->left != tree->nil) {
        node = node->left;
    }
    return node;
}

static void clear_node(infra_rbtree_t* tree, infra_rbtree_node_t* node) {
    if (node != NULL && node != tree->nil) {
        clear_node(tree, node->left);
        clear_node(tree, node->right);
        infra_free(node);
    }
}

static infra_rbtree_node_t* create_node(infra_rbtree_t* tree, int key, void* value) {
    infra_rbtree_node_t* node = (infra_rbtree_node_t*)infra_malloc(sizeof(infra_rbtree_node_t));
    if (node != NULL) {
        node->key = key;
        node->value = value;
        node->color = INFRA_RBTREE_RED;
        node->parent = tree->nil;
        node->left = tree->nil;
        node->right = tree->nil;
    }
    return node;
}

static void rotate_left(infra_rbtree_t* tree, infra_rbtree_node_t* node) {
    infra_rbtree_node_t* right = node->right;
    node->right = right->left;

    if (right->left != tree->nil) {
        right->left->parent = node;
    }

    right->parent = node->parent;

    if (node->parent == tree->nil) {
        tree->root = right;
    } else if (node == node->parent->left) {
        node->parent->left = right;
    } else {
        node->parent->right = right;
    }

    right->left = node;
    node->parent = right;
}

static void rotate_right(infra_rbtree_t* tree, infra_rbtree_node_t* node) {
    infra_rbtree_node_t* left = node->left;
    node->left = left->right;

    if (left->right != tree->nil) {
        left->right->parent = node;
    }

    left->parent = node->parent;

    if (node->parent == tree->nil) {
        tree->root = left;
    } else if (node == node->parent->right) {
        node->parent->right = left;
    } else {
        node->parent->left = left;
    }

    left->right = node;
    node->parent = left;
}

static void fix_insert(infra_rbtree_t* tree, infra_rbtree_node_t* node) {
    while (node != tree->root && node->parent->color == INFRA_RBTREE_RED) {
        if (node->parent == node->parent->parent->left) {
            infra_rbtree_node_t* uncle = node->parent->parent->right;
            if (uncle != tree->nil && uncle->color == INFRA_RBTREE_RED) {
                node->parent->color = INFRA_RBTREE_BLACK;
                uncle->color = INFRA_RBTREE_BLACK;
                node->parent->parent->color = INFRA_RBTREE_RED;
                node = node->parent->parent;
            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    rotate_left(tree, node);
                }
                node->parent->color = INFRA_RBTREE_BLACK;
                node->parent->parent->color = INFRA_RBTREE_RED;
                rotate_right(tree, node->parent->parent);
            }
        } else {
            infra_rbtree_node_t* uncle = node->parent->parent->left;
            if (uncle != tree->nil && uncle->color == INFRA_RBTREE_RED) {
                node->parent->color = INFRA_RBTREE_BLACK;
                uncle->color = INFRA_RBTREE_BLACK;
                node->parent->parent->color = INFRA_RBTREE_RED;
                node = node->parent->parent;
            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    rotate_right(tree, node);
                }
                node->parent->color = INFRA_RBTREE_BLACK;
                node->parent->parent->color = INFRA_RBTREE_RED;
                rotate_left(tree, node->parent->parent);
            }
        }
    }
    tree->root->color = INFRA_RBTREE_BLACK;
}

static void fix_delete(infra_rbtree_t* tree, infra_rbtree_node_t* node) {
    while (node != tree->root && node->color == INFRA_RBTREE_BLACK) {
        if (node == node->parent->left) {
            infra_rbtree_node_t* sibling = node->parent->right;
            if (sibling == tree->nil) {
                node = node->parent;
                continue;
            }
            if (sibling->color == INFRA_RBTREE_RED) {
                sibling->color = INFRA_RBTREE_BLACK;
                node->parent->color = INFRA_RBTREE_RED;
                rotate_left(tree, node->parent);
                sibling = node->parent->right;
            }
            if ((sibling->left == tree->nil || sibling->left->color == INFRA_RBTREE_BLACK) &&
                (sibling->right == tree->nil || sibling->right->color == INFRA_RBTREE_BLACK)) {
                sibling->color = INFRA_RBTREE_RED;
                node = node->parent;
            } else {
                if (sibling->right == tree->nil || sibling->right->color == INFRA_RBTREE_BLACK) {
                    if (sibling->left != tree->nil) {
                        sibling->left->color = INFRA_RBTREE_BLACK;
                    }
                    sibling->color = INFRA_RBTREE_RED;
                    rotate_right(tree, sibling);
                    sibling = node->parent->right;
                }
                sibling->color = node->parent->color;
                node->parent->color = INFRA_RBTREE_BLACK;
                if (sibling->right != tree->nil) {
                    sibling->right->color = INFRA_RBTREE_BLACK;
                }
                rotate_left(tree, node->parent);
                node = tree->root;
            }
        } else {
            infra_rbtree_node_t* sibling = node->parent->left;
            if (sibling == tree->nil) {
                node = node->parent;
                continue;
            }
            if (sibling->color == INFRA_RBTREE_RED) {
                sibling->color = INFRA_RBTREE_BLACK;
                node->parent->color = INFRA_RBTREE_RED;
                rotate_right(tree, node->parent);
                sibling = node->parent->left;
            }
            if ((sibling->right == tree->nil || sibling->right->color == INFRA_RBTREE_BLACK) &&
                (sibling->left == tree->nil || sibling->left->color == INFRA_RBTREE_BLACK)) {
                sibling->color = INFRA_RBTREE_RED;
                node = node->parent;
            } else {
                if (sibling->left == tree->nil || sibling->left->color == INFRA_RBTREE_BLACK) {
                    if (sibling->right != tree->nil) {
                        sibling->right->color = INFRA_RBTREE_BLACK;
                    }
                    sibling->color = INFRA_RBTREE_RED;
                    rotate_left(tree, sibling);
                    sibling = node->parent->left;
                }
                sibling->color = node->parent->color;
                node->parent->color = INFRA_RBTREE_BLACK;
                if (sibling->left != tree->nil) {
                    sibling->left->color = INFRA_RBTREE_BLACK;
                }
                rotate_right(tree, node->parent);
                node = tree->root;
            }
        }
    }
    node->color = INFRA_RBTREE_BLACK;
}

infra_error_t infra_rbtree_create(infra_rbtree_t** tree) {
    if (tree == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *tree = (infra_rbtree_t*)infra_malloc(sizeof(infra_rbtree_t));
    if (*tree == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 创建哨兵节点
    (*tree)->nil = (infra_rbtree_node_t*)infra_malloc(sizeof(infra_rbtree_node_t));
    if ((*tree)->nil == NULL) {
        infra_free(*tree);
        return INFRA_ERROR_NO_MEMORY;
    }

    (*tree)->nil->color = INFRA_RBTREE_BLACK;
    (*tree)->nil->left = NULL;
    (*tree)->nil->right = NULL;
    (*tree)->nil->parent = NULL;
    (*tree)->root = (*tree)->nil;
    (*tree)->size = 0;

    return INFRA_OK;
}

infra_error_t infra_rbtree_insert(infra_rbtree_t* tree, int key, void* value) {
    if (tree == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_rbtree_node_t* node = create_node(tree, key, value);
    if (node == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }

    infra_rbtree_node_t* parent = tree->nil;
    infra_rbtree_node_t* current = tree->root;

    while (current != tree->nil) {
        parent = current;
        if (key < current->key) {
            current = current->left;
        } else if (key > current->key) {
            current = current->right;
        } else {
            // 键已存在，更新值
            current->value = value;
            infra_free(node);
            return INFRA_OK;
        }
    }

    node->parent = parent;

    if (parent == tree->nil) {
        tree->root = node;
    } else if (key < parent->key) {
        parent->left = node;
    } else {
        parent->right = node;
    }

    fix_insert(tree, node);
    tree->size++;

    return INFRA_OK;
}

void* infra_rbtree_find(infra_rbtree_t* tree, int key) {
    if (tree == NULL) {
        return NULL;
    }

    infra_rbtree_node_t* node = tree->root;
    while (node != tree->nil) {
        if (key < node->key) {
            node = node->left;
        } else if (key > node->key) {
            node = node->right;
        } else {
            return node->value;
        }
    }

    return NULL;
}

void* infra_rbtree_remove(infra_rbtree_t* tree, int key) {
    if (tree == NULL) {
        return NULL;
    }

    infra_rbtree_node_t* node = tree->root;
    while (node != tree->nil) {
        if (key < node->key) {
            node = node->left;
        } else if (key > node->key) {
            node = node->right;
        } else {
            void* value = node->value;
            
            infra_rbtree_node_t* x;
            infra_rbtree_node_t* y = node;
            infra_rbtree_color_t y_original_color = y->color;

            if (node->left == tree->nil) {
                x = node->right;
                transplant(tree, node, node->right);
            } else if (node->right == tree->nil) {
                x = node->left;
                transplant(tree, node, node->left);
            } else {
                y = minimum(tree, node->right);
                y_original_color = y->color;
                x = y->right;
                
                if (y->parent == node) {
                    x->parent = y;
                } else {
                    transplant(tree, y, y->right);
                    y->right = node->right;
                    y->right->parent = y;
                }
                
                transplant(tree, node, y);
                y->left = node->left;
                y->left->parent = y;
                y->color = node->color;
            }

            if (y_original_color == INFRA_RBTREE_BLACK) {
                fix_delete(tree, x);
            }

            infra_free(node);
            tree->size--;
            return value;
        }
    }

    return NULL;
}

void infra_rbtree_clear(infra_rbtree_t* tree) {
    if (tree != NULL) {
        clear_node(tree, tree->root);
        tree->root = tree->nil;
        tree->size = 0;
    }
}

void infra_rbtree_destroy(infra_rbtree_t* tree) {
    if (tree != NULL) {
        infra_rbtree_clear(tree);
        infra_free(tree->nil);
        infra_free(tree);
    }
} 