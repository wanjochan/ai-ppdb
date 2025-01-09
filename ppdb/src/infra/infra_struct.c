#include "cosmopolitan.h"
#include "internal/infra/infra.h"

/* Linked List Implementation */
void infra_list_init(struct infra_list* list) {
    list->next = list;
    list->prev = list;
}

void infra_list_add(struct infra_list* list, struct infra_list* node) {
    node->next = list->next;
    node->prev = list;
    list->next->prev = node;
    list->next = node;
}

void infra_list_del(struct infra_list* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

int infra_list_empty(struct infra_list* list) {
    return list->next == list;
}

/* Hash Table Implementation */
static u64 hash_bytes(const void* key, size_t len) {
    const u8* data = (const u8*)key;
    u64 hash = 14695981039346656037ULL; /* FNV-1a hash */
    
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    
    return hash;
}

int infra_hash_init(struct infra_hash_table* table, size_t nbuckets) {
    if (!table || nbuckets == 0) {
        return PPDB_ERR_PARAM;
    }

    table->buckets = malloc(nbuckets * sizeof(struct infra_list));
    if (!table->buckets) {
        return PPDB_ERR_MEMORY;
    }

    for (size_t i = 0; i < nbuckets; i++) {
        infra_list_init(&table->buckets[i]);
    }

    table->nbuckets = nbuckets;
    table->size = 0;

    return PPDB_OK;
}

void infra_hash_destroy(struct infra_hash_table* table) {
    if (!table) {
        return;
    }

    for (size_t i = 0; i < table->nbuckets; i++) {
        struct infra_list* bucket = &table->buckets[i];
        struct infra_list* pos = bucket->next;
        while (pos != bucket) {
            struct infra_list* next = pos->next;
            struct infra_hash_node* node = container_of(pos, struct infra_hash_node, list);
            free(node);
            pos = next;
        }
    }

    free(table->buckets);
    table->buckets = NULL;
    table->nbuckets = 0;
    table->size = 0;
}

int infra_hash_put(struct infra_hash_table* table, void* key, size_t klen, void* value) {
    if (!table || !key) {
        return PPDB_ERR_PARAM;
    }

    u64 hash = hash_bytes(key, klen);
    size_t bucket_idx = hash % table->nbuckets;
    struct infra_list* bucket = &table->buckets[bucket_idx];

    // Check if key already exists
    struct infra_list* pos;
    for (pos = bucket->next; pos != bucket; pos = pos->next) {
        struct infra_hash_node* node = container_of(pos, struct infra_hash_node, list);
        if (node->hash == hash && node->key == key) {
            node->value = value;
            return PPDB_OK;
        }
    }

    // Create new node
    struct infra_hash_node* node = malloc(sizeof(struct infra_hash_node));
    if (!node) {
        return PPDB_ERR_MEMORY;
    }

    node->hash = hash;
    node->key = key;
    node->value = value;
    infra_list_init(&node->list);
    infra_list_add(bucket, &node->list);
    table->size++;

    return PPDB_OK;
}

void* infra_hash_get(struct infra_hash_table* table, void* key, size_t klen) {
    if (!table || !key) {
        return NULL;
    }

    u64 hash = hash_bytes(key, klen);
    size_t bucket_idx = hash % table->nbuckets;
    struct infra_list* bucket = &table->buckets[bucket_idx];

    struct infra_list* pos;
    for (pos = bucket->next; pos != bucket; pos = pos->next) {
        struct infra_hash_node* node = container_of(pos, struct infra_hash_node, list);
        if (node->hash == hash && node->key == key) {
            return node->value;
        }
    }

    return NULL;
}

int infra_hash_del(struct infra_hash_table* table, void* key, size_t klen) {
    if (!table || !key) {
        return PPDB_ERR_PARAM;
    }

    u64 hash = hash_bytes(key, klen);
    size_t bucket_idx = hash % table->nbuckets;
    struct infra_list* bucket = &table->buckets[bucket_idx];

    struct infra_list* pos;
    for (pos = bucket->next; pos != bucket; pos = pos->next) {
        struct infra_hash_node* node = container_of(pos, struct infra_hash_node, list);
        if (node->hash == hash && node->key == key) {
            infra_list_del(&node->list);
            free(node);
            table->size--;
            return PPDB_OK;
        }
    }

    return PPDB_ERR_NOTFOUND;
}

/* Queue Implementation */
void infra_queue_init(struct infra_queue* queue) {
    if (!queue) {
        return;
    }
    infra_list_init(&queue->list);
    queue->size = 0;
}

int infra_queue_push(struct infra_queue* queue, void* data) {
    if (!queue) {
        return PPDB_ERR_PARAM;
    }

    struct infra_queue_node* node = malloc(sizeof(struct infra_queue_node));
    if (!node) {
        return PPDB_ERR_MEMORY;
    }

    node->data = data;
    infra_list_init(&node->list);
    infra_list_add(queue->list.prev, &node->list);
    queue->size++;

    return PPDB_OK;
}

void* infra_queue_pop(struct infra_queue* queue) {
    if (!queue || infra_queue_empty(queue)) {
        return NULL;
    }

    struct infra_list* first = queue->list.next;
    struct infra_queue_node* node = container_of(first, struct infra_queue_node, list);
    void* data = node->data;

    infra_list_del(first);
    free(node);
    queue->size--;

    return data;
}

int infra_queue_empty(struct infra_queue* queue) {
    return !queue || infra_list_empty(&queue->list);
}

size_t infra_queue_size(struct infra_queue* queue) {
    return queue ? queue->size : 0;
}

/* Red-Black Tree Implementation */
static void rb_set_parent(struct infra_rb_node* node, struct infra_rb_node* parent) {
    if (node) {
        node->parent = parent;
    }
}

static void rb_set_color(struct infra_rb_node* node, int color) {
    if (node) {
        node->color = color;
    }
}

static int rb_is_black(struct infra_rb_node* node) {
    return !node || node->color == INFRA_RB_BLACK;
}

static void rb_rotate_left(struct infra_rb_tree* tree, struct infra_rb_node* node) {
    struct infra_rb_node* right = node->right;
    struct infra_rb_node* parent = node->parent;

    node->right = right->left;
    if (right->left) {
        right->left->parent = node;
    }

    right->left = node;
    right->parent = parent;

    if (!parent) {
        tree->root = right;
    } else if (parent->left == node) {
        parent->left = right;
    } else {
        parent->right = right;
    }

    node->parent = right;
}

static void rb_rotate_right(struct infra_rb_tree* tree, struct infra_rb_node* node) {
    struct infra_rb_node* left = node->left;
    struct infra_rb_node* parent = node->parent;

    node->left = left->right;
    if (left->right) {
        left->right->parent = node;
    }

    left->right = node;
    left->parent = parent;

    if (!parent) {
        tree->root = left;
    } else if (parent->left == node) {
        parent->left = left;
    } else {
        parent->right = left;
    }

    node->parent = left;
}

static struct infra_rb_node* rb_minimum(struct infra_rb_node* node) {
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

static void rb_insert_color(struct infra_rb_tree* tree, struct infra_rb_node* node) {
    struct infra_rb_node* parent, *gparent, *uncle;

    while ((parent = node->parent) && parent->color == INFRA_RB_RED) {
        gparent = parent->parent;

        if (parent == gparent->left) {
            uncle = gparent->right;
            if (uncle && uncle->color == INFRA_RB_RED) {
                uncle->color = INFRA_RB_BLACK;
                parent->color = INFRA_RB_BLACK;
                gparent->color = INFRA_RB_RED;
                node = gparent;
                continue;
            }

            if (parent->right == node) {
                rb_rotate_left(tree, parent);
                struct infra_rb_node* tmp = parent;
                parent = node;
                node = tmp;
            }

            parent->color = INFRA_RB_BLACK;
            gparent->color = INFRA_RB_RED;
            rb_rotate_right(tree, gparent);
        } else {
            uncle = gparent->left;
            if (uncle && uncle->color == INFRA_RB_RED) {
                uncle->color = INFRA_RB_BLACK;
                parent->color = INFRA_RB_BLACK;
                gparent->color = INFRA_RB_RED;
                node = gparent;
                continue;
            }

            if (parent->left == node) {
                rb_rotate_right(tree, parent);
                struct infra_rb_node* tmp = parent;
                parent = node;
                node = tmp;
            }

            parent->color = INFRA_RB_BLACK;
            gparent->color = INFRA_RB_RED;
            rb_rotate_left(tree, gparent);
        }
    }

    tree->root->color = INFRA_RB_BLACK;
}

void infra_rbtree_init(struct infra_rb_tree* tree) {
    if (!tree) {
        return;
    }
    tree->root = NULL;
    tree->size = 0;
}

int infra_rbtree_insert(struct infra_rb_tree* tree, struct infra_rb_node* node,
                       int (*cmp)(struct infra_rb_node*, struct infra_rb_node*)) {
    if (!tree || !node || !cmp) {
        return PPDB_ERR_PARAM;
    }

    struct infra_rb_node* parent = NULL;
    struct infra_rb_node** p = &tree->root;

    while (*p) {
        parent = *p;
        int result = cmp(node, parent);
        if (result < 0) {
            p = &parent->left;
        } else if (result > 0) {
            p = &parent->right;
        } else {
            return PPDB_ERR_EXISTS;
        }
    }

    node->parent = parent;
    node->left = NULL;
    node->right = NULL;
    node->color = INFRA_RB_RED;

    *p = node;
    tree->size++;

    rb_insert_color(tree, node);

    return PPDB_OK;
}

struct infra_rb_node* infra_rbtree_find(struct infra_rb_tree* tree, struct infra_rb_node* key,
                                       int (*cmp)(struct infra_rb_node*, struct infra_rb_node*)) {
    if (!tree || !key || !cmp) {
        return NULL;
    }

    struct infra_rb_node* node = tree->root;
    while (node) {
        int result = cmp(key, node);
        if (result < 0) {
            node = node->left;
        } else if (result > 0) {
            node = node->right;
        } else {
            return node;
        }
    }

    return NULL;
}

size_t infra_rbtree_size(struct infra_rb_tree* tree) {
    return tree ? tree->size : 0;
}
