/*
 * infra_struct.c - Data Structure Implementation
 */

#include "internal/infra/infra_core.h"

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
// Hash Operations Implementation
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

    (*hash)->buckets = (infra_hash_node_t**)infra_calloc(capacity, sizeof(infra_hash_node_t*));
    if ((*hash)->buckets == NULL) {
        infra_free(*hash);
        return INFRA_ERROR_NO_MEMORY;
    }

    (*hash)->size = 0;
    (*hash)->capacity = capacity;

    return INFRA_OK;
}

void infra_hash_destroy(infra_hash_t* hash) {
    if (hash == NULL) {
        return;
    }

    infra_hash_clear(hash);
    infra_free(hash->buckets);
    infra_free(hash);
}

infra_error_t infra_hash_put(infra_hash_t* hash, const char* key, void* value) {
    if (hash == NULL || key == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t index = hash_string(key) % hash->capacity;
    infra_hash_node_t* node = hash->buckets[index];

    while (node != NULL) {
        if (infra_strcmp(node->key, key) == 0) {
            node->value = value;
            return INFRA_OK;
        }
        node = node->next;
    }

    node = (infra_hash_node_t*)infra_malloc(sizeof(infra_hash_node_t));
    if (node == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }

    node->key = infra_strdup(key);
    if (node->key == NULL) {
        infra_free(node);
        return INFRA_ERROR_NO_MEMORY;
    }

    node->value = value;
    node->next = hash->buckets[index];
    hash->buckets[index] = node;
    hash->size++;

    return INFRA_OK;
}

void* infra_hash_get(infra_hash_t* hash, const char* key) {
    if (hash == NULL || key == NULL) {
        return NULL;
    }

    size_t index = hash_string(key) % hash->capacity;
    infra_hash_node_t* node = hash->buckets[index];

    while (node != NULL) {
        if (infra_strcmp(node->key, key) == 0) {
            return node->value;
        }
        node = node->next;
    }

    return NULL;
}

void* infra_hash_remove(infra_hash_t* hash, const char* key) {
    if (hash == NULL || key == NULL) {
        return NULL;
    }

    size_t index = hash_string(key) % hash->capacity;
    infra_hash_node_t* node = hash->buckets[index];
    infra_hash_node_t* prev = NULL;
    void* value = NULL;

    while (node != NULL) {
        if (infra_strcmp(node->key, key) == 0) {
            if (prev == NULL) {
                hash->buckets[index] = node->next;
            } else {
                prev->next = node->next;
            }
            value = node->value;
            infra_free(node->key);
            infra_free(node);
            hash->size--;
            break;
        }
        prev = node;
        node = node->next;
    }

    return value;
}

void infra_hash_clear(infra_hash_t* hash) {
    if (hash == NULL) {
        return;
    }

    for (size_t i = 0; i < hash->capacity; i++) {
        infra_hash_node_t* node = hash->buckets[i];
        while (node != NULL) {
            infra_hash_node_t* next = node->next;
            infra_free(node->key);
            infra_free(node);
            node = next;
        }
        hash->buckets[i] = NULL;
    }
    hash->size = 0;
}

//-----------------------------------------------------------------------------
// Red-Black Tree Operations Implementation
//-----------------------------------------------------------------------------

static infra_rbtree_node_t* create_node(int key, void* value) {
    infra_rbtree_node_t* node = (infra_rbtree_node_t*)infra_malloc(sizeof(infra_rbtree_node_t));
    if (node != NULL) {
        node->key = key;
        node->value = value;
        node->color = INFRA_RBTREE_RED;
        node->parent = NULL;
        node->left = NULL;
        node->right = NULL;
    }
    return node;
}

static void rotate_left(infra_rbtree_t* tree, infra_rbtree_node_t* node) {
    infra_rbtree_node_t* right = node->right;
    node->right = right->left;
    
    if (right->left != NULL) {
        right->left->parent = node;
    }
    
    right->parent = node->parent;
    
    if (node->parent == NULL) {
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
    
    if (left->right != NULL) {
        left->right->parent = node;
    }
    
    left->parent = node->parent;
    
    if (node->parent == NULL) {
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
    infra_rbtree_node_t* parent = NULL;
    infra_rbtree_node_t* grandparent = NULL;
    
    while ((node != tree->root) && (node->color != INFRA_RBTREE_BLACK) &&
           (node->parent->color == INFRA_RBTREE_RED)) {
        parent = node->parent;
        grandparent = parent->parent;
        
        if (parent == grandparent->left) {
            infra_rbtree_node_t* uncle = grandparent->right;
            
            if (uncle != NULL && uncle->color == INFRA_RBTREE_RED) {
                grandparent->color = INFRA_RBTREE_RED;
                parent->color = INFRA_RBTREE_BLACK;
                uncle->color = INFRA_RBTREE_BLACK;
                node = grandparent;
            } else {
                if (node == parent->right) {
                    rotate_left(tree, parent);
                    node = parent;
                    parent = node->parent;
                }
                rotate_right(tree, grandparent);
                infra_rbtree_color_t temp = parent->color;
                parent->color = grandparent->color;
                grandparent->color = temp;
                node = parent;
            }
        } else {
            infra_rbtree_node_t* uncle = grandparent->left;
            
            if (uncle != NULL && uncle->color == INFRA_RBTREE_RED) {
                grandparent->color = INFRA_RBTREE_RED;
                parent->color = INFRA_RBTREE_BLACK;
                uncle->color = INFRA_RBTREE_BLACK;
                node = grandparent;
            } else {
                if (node == parent->left) {
                    rotate_right(tree, parent);
                    node = parent;
                    parent = node->parent;
                }
                rotate_left(tree, grandparent);
                infra_rbtree_color_t temp = parent->color;
                parent->color = grandparent->color;
                grandparent->color = temp;
                node = parent;
            }
        }
    }
    
    tree->root->color = INFRA_RBTREE_BLACK;
}

infra_error_t infra_rbtree_create(infra_rbtree_t** tree) {
    if (tree == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *tree = (infra_rbtree_t*)infra_malloc(sizeof(infra_rbtree_t));
    if (*tree == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }

    (*tree)->root = NULL;
    (*tree)->size = 0;

    return INFRA_OK;
}

void infra_rbtree_destroy(infra_rbtree_t* tree) {
    if (tree == NULL) {
        return;
    }
    infra_rbtree_clear(tree);
    infra_free(tree);
}

infra_error_t infra_rbtree_insert(infra_rbtree_t* tree, int key, void* value) {
    if (tree == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_rbtree_node_t* node = create_node(key, value);
    if (node == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }

    if (tree->root == NULL) {
        node->color = INFRA_RBTREE_BLACK;
        tree->root = node;
    } else {
        infra_rbtree_node_t* current = tree->root;
        infra_rbtree_node_t* parent = NULL;

        while (current != NULL) {
            parent = current;
            if (key < current->key) {
                current = current->left;
            } else if (key > current->key) {
                current = current->right;
            } else {
                current->value = value;
                infra_free(node);
                return INFRA_OK;
            }
        }

        node->parent = parent;
        if (key < parent->key) {
            parent->left = node;
        } else {
            parent->right = node;
        }

        fix_insert(tree, node);
    }

    tree->size++;
    return INFRA_OK;
}

void* infra_rbtree_find(infra_rbtree_t* tree, int key) {
    if (tree == NULL) {
        return NULL;
    }

    infra_rbtree_node_t* current = tree->root;
    while (current != NULL) {
        if (key < current->key) {
            current = current->left;
        } else if (key > current->key) {
            current = current->right;
        } else {
            return current->value;
        }
    }

    return NULL;
}

static infra_rbtree_node_t* find_min(infra_rbtree_node_t* node) {
    while (node->left != NULL) {
        node = node->left;
    }
    return node;
}

static void fix_delete(infra_rbtree_t* tree, infra_rbtree_node_t* node) {
    while (node != tree->root && node->color == INFRA_RBTREE_BLACK) {
        if (node == node->parent->left) {
            infra_rbtree_node_t* sibling = node->parent->right;
            if (sibling->color == INFRA_RBTREE_RED) {
                sibling->color = INFRA_RBTREE_BLACK;
                node->parent->color = INFRA_RBTREE_RED;
                rotate_left(tree, node->parent);
                sibling = node->parent->right;
            }
            if (sibling->left->color == INFRA_RBTREE_BLACK && 
                sibling->right->color == INFRA_RBTREE_BLACK) {
                sibling->color = INFRA_RBTREE_RED;
                node = node->parent;
            } else {
                if (sibling->right->color == INFRA_RBTREE_BLACK) {
                    sibling->left->color = INFRA_RBTREE_BLACK;
                    sibling->color = INFRA_RBTREE_RED;
                    rotate_right(tree, sibling);
                    sibling = node->parent->right;
                }
                sibling->color = node->parent->color;
                node->parent->color = INFRA_RBTREE_BLACK;
                sibling->right->color = INFRA_RBTREE_BLACK;
                rotate_left(tree, node->parent);
                node = tree->root;
            }
        } else {
            infra_rbtree_node_t* sibling = node->parent->left;
            if (sibling->color == INFRA_RBTREE_RED) {
                sibling->color = INFRA_RBTREE_BLACK;
                node->parent->color = INFRA_RBTREE_RED;
                rotate_right(tree, node->parent);
                sibling = node->parent->left;
            }
            if (sibling->right->color == INFRA_RBTREE_BLACK && 
                sibling->left->color == INFRA_RBTREE_BLACK) {
                sibling->color = INFRA_RBTREE_RED;
                node = node->parent;
            } else {
                if (sibling->left->color == INFRA_RBTREE_BLACK) {
                    sibling->right->color = INFRA_RBTREE_BLACK;
                    sibling->color = INFRA_RBTREE_RED;
                    rotate_left(tree, sibling);
                    sibling = node->parent->left;
                }
                sibling->color = node->parent->color;
                node->parent->color = INFRA_RBTREE_BLACK;
                sibling->left->color = INFRA_RBTREE_BLACK;
                rotate_right(tree, node->parent);
                node = tree->root;
            }
        }
    }
    node->color = INFRA_RBTREE_BLACK;
}

void* infra_rbtree_remove(infra_rbtree_t* tree, int key) {
    if (tree == NULL || tree->root == NULL) {
        return NULL;
    }

    infra_rbtree_node_t* node = tree->root;
    infra_rbtree_node_t* target = NULL;
    infra_rbtree_node_t* temp = NULL;
    void* value = NULL;

    while (node != NULL) {
        if (key < node->key) {
            node = node->left;
        } else if (key > node->key) {
            node = node->right;
        } else {
            target = node;
            value = node->value;
            break;
        }
    }

    if (target == NULL) {
        return NULL;
    }

    if (target->left == NULL || target->right == NULL) {
        temp = target;
    } else {
        temp = find_min(target->right);
        target->key = temp->key;
        target->value = temp->value;
    }

    infra_rbtree_node_t* child = temp->left != NULL ? temp->left : temp->right;
    infra_rbtree_node_t* parent = temp->parent;

    if (child != NULL) {
        child->parent = parent;
    }

    if (parent == NULL) {
        tree->root = child;
    } else if (temp == parent->left) {
        parent->left = child;
    } else {
        parent->right = child;
    }

    if (temp->color == INFRA_RBTREE_BLACK && child != NULL) {
        fix_delete(tree, child);
    }

    infra_free(temp);
    tree->size--;

    return value;
}

void infra_rbtree_clear(infra_rbtree_t* tree) {
    if (tree == NULL) {
        return;
    }

    infra_rbtree_node_t* current = tree->root;
    infra_rbtree_node_t* temp;

    while (current != NULL) {
        if (current->left == NULL) {
            temp = current;
            current = current->right;
            infra_free(temp);
        } else {
            temp = current->left;
            while (temp->right != NULL && temp->right != current) {
                temp = temp->right;
            }
            if (temp->right == NULL) {
                temp->right = current;
                current = current->left;
            } else {
                temp->right = NULL;
                temp = current;
                current = current->right;
                infra_free(temp);
            }
        }
    }

    tree->root = NULL;
    tree->size = 0;
} 