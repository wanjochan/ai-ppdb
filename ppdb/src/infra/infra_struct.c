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
    table->buckets = infra_calloc(nbuckets, sizeof(struct infra_list));
    if (!table->buckets) {
        return INFRA_ERR_NOMEM;
    }
    
    table->nbuckets = nbuckets;
    table->size = 0;
    
    for (size_t i = 0; i < nbuckets; i++) {
        infra_list_init(&table->buckets[i]);
    }
    
    return INFRA_OK;
}

void infra_hash_destroy(struct infra_hash_table* table) {
    if (!table->buckets) return;
    
    for (size_t i = 0; i < table->nbuckets; i++) {
        struct infra_list* head = &table->buckets[i];
        while (!infra_list_empty(head)) {
            struct infra_hash_node* node = (struct infra_hash_node*)head->next;
            infra_list_del(&node->list);
            infra_free(node);
        }
    }
    
    infra_free(table->buckets);
    table->buckets = NULL;
    table->size = 0;
}

int infra_hash_put(struct infra_hash_table* table, void* key, size_t klen, void* value) {
    u64 hash = hash_bytes(key, klen);
    size_t bucket = hash % table->nbuckets;
    struct infra_list* head = &table->buckets[bucket];
    
    /* Check for existing key */
    struct infra_list* pos;
    for (pos = head->next; pos != head; pos = pos->next) {
        struct infra_hash_node* node = (struct infra_hash_node*)pos;
        if (node->hash == hash && memcmp(node->key, key, klen) == 0) {
            node->value = value;
            return INFRA_OK;
        }
    }
    
    /* Create new node */
    struct infra_hash_node* node = infra_malloc(sizeof(*node));
    if (!node) {
        return INFRA_ERR_NOMEM;
    }
    
    node->hash = hash;
    node->key = key;
    node->value = value;
    infra_list_add(head, &node->list);
    table->size++;
    
    return INFRA_OK;
}

void* infra_hash_get(struct infra_hash_table* table, void* key, size_t klen) {
    u64 hash = hash_bytes(key, klen);
    size_t bucket = hash % table->nbuckets;
    struct infra_list* head = &table->buckets[bucket];
    
    struct infra_list* pos;
    for (pos = head->next; pos != head; pos = pos->next) {
        struct infra_hash_node* node = (struct infra_hash_node*)pos;
        if (node->hash == hash && memcmp(node->key, key, klen) == 0) {
            return node->value;
        }
    }
    
    return NULL;
}

int infra_hash_del(struct infra_hash_table* table, void* key, size_t klen) {
    u64 hash = hash_bytes(key, klen);
    size_t bucket = hash % table->nbuckets;
    struct infra_list* head = &table->buckets[bucket];
    
    struct infra_list* pos;
    for (pos = head->next; pos != head; pos = pos->next) {
        struct infra_hash_node* node = (struct infra_hash_node*)pos;
        if (node->hash == hash && memcmp(node->key, key, klen) == 0) {
            infra_list_del(&node->list);
            infra_free(node);
            table->size--;
            return INFRA_OK;
        }
    }
    
    return INFRA_ERR_NOTFOUND;
}

/* Queue Implementation */
void infra_queue_init(struct infra_queue* queue) {
    infra_list_init(&queue->list);
    queue->size = 0;
}

int infra_queue_push(struct infra_queue* queue, void* data) {
    struct infra_queue_node* node = infra_malloc(sizeof(*node));
    if (!node) {
        return INFRA_ERR_NOMEM;
    }
    
    node->data = data;
    infra_list_add(&queue->list, &node->list);
    queue->size++;
    
    return INFRA_OK;
}

void* infra_queue_pop(struct infra_queue* queue) {
    if (infra_queue_empty(queue)) {
        return NULL;
    }
    
    struct infra_queue_node* node = (struct infra_queue_node*)queue->list.prev;
    void* data = node->data;
    
    infra_list_del(&node->list);
    infra_free(node);
    queue->size--;
    
    return data;
}

int infra_queue_empty(struct infra_queue* queue) {
    return queue->size == 0;
}

size_t infra_queue_size(struct infra_queue* queue) {
    return queue->size;
}

/* Red-Black Tree Implementation */
static void rb_set_parent(struct infra_rb_node* node, struct infra_rb_node* parent) {
    node->parent = parent;
}

static void rb_set_color(struct infra_rb_node* node, int color) {
    node->color = color;
}

static struct infra_rb_node* rb_parent(struct infra_rb_node* node) {
    return node->parent;
}

static int rb_is_red(struct infra_rb_node* node) {
    return node && node->color == INFRA_RB_RED;
}

static int rb_is_black(struct infra_rb_node* node) {
    return !rb_is_red(node);
}

static void rb_set_red(struct infra_rb_node* node) {
    rb_set_color(node, INFRA_RB_RED);
}

static void rb_set_black(struct infra_rb_node* node) {
    rb_set_color(node, INFRA_RB_BLACK);
}

static struct infra_rb_node* rb_minimum(struct infra_rb_node* node) {
    while (node->left)
        node = node->left;
    return node;
}

static void rb_rotate_left(struct infra_rb_tree* tree, struct infra_rb_node* node) {
    struct infra_rb_node* right = node->right;
    
    node->right = right->left;
    if (right->left)
        rb_set_parent(right->left, node);
    
    rb_set_parent(right, rb_parent(node));
    
    if (!rb_parent(node))
        tree->root = right;
    else if (node == rb_parent(node)->left)
        rb_parent(node)->left = right;
    else
        rb_parent(node)->right = right;
    
    right->left = node;
    rb_set_parent(node, right);
}

static void rb_rotate_right(struct infra_rb_tree* tree, struct infra_rb_node* node) {
    struct infra_rb_node* left = node->left;
    
    node->left = left->right;
    if (left->right)
        rb_set_parent(left->right, node);
    
    rb_set_parent(left, rb_parent(node));
    
    if (!rb_parent(node))
        tree->root = left;
    else if (node == rb_parent(node)->right)
        rb_parent(node)->right = left;
    else
        rb_parent(node)->left = left;
    
    left->right = node;
    rb_set_parent(node, left);
}

static void rb_insert_fixup(struct infra_rb_tree* tree, struct infra_rb_node* node) {
    while (rb_is_red(rb_parent(node))) {
        if (rb_parent(node) == rb_parent(rb_parent(node))->left) {
            struct infra_rb_node* uncle = rb_parent(rb_parent(node))->right;
            if (rb_is_red(uncle)) {
                rb_set_black(rb_parent(node));
                rb_set_black(uncle);
                rb_set_red(rb_parent(rb_parent(node)));
                node = rb_parent(rb_parent(node));
            } else {
                if (node == rb_parent(node)->right) {
                    node = rb_parent(node);
                    rb_rotate_left(tree, node);
                }
                rb_set_black(rb_parent(node));
                rb_set_red(rb_parent(rb_parent(node)));
                rb_rotate_right(tree, rb_parent(rb_parent(node)));
            }
        } else {
            struct infra_rb_node* uncle = rb_parent(rb_parent(node))->left;
            if (rb_is_red(uncle)) {
                rb_set_black(rb_parent(node));
                rb_set_black(uncle);
                rb_set_red(rb_parent(rb_parent(node)));
                node = rb_parent(rb_parent(node));
            } else {
                if (node == rb_parent(node)->left) {
                    node = rb_parent(node);
                    rb_rotate_right(tree, node);
                }
                rb_set_black(rb_parent(node));
                rb_set_red(rb_parent(rb_parent(node)));
                rb_rotate_left(tree, rb_parent(rb_parent(node)));
            }
        }
    }
    rb_set_black(tree->root);
}

void infra_rbtree_init(struct infra_rb_tree* tree) {
    tree->root = NULL;
    tree->size = 0;
}

int infra_rbtree_insert(struct infra_rb_tree* tree, struct infra_rb_node* node,
                       int (*cmp)(struct infra_rb_node*, struct infra_rb_node*)) {
    struct infra_rb_node* parent = NULL;
    struct infra_rb_node** p = &tree->root;
    
    while (*p) {
        parent = *p;
        int res = cmp(node, parent);
        if (res < 0)
            p = &parent->left;
        else if (res > 0)
            p = &parent->right;
        else
            return INFRA_ERR_EXISTS;
    }
    
    node->left = node->right = NULL;
    rb_set_parent(node, parent);
    rb_set_red(node);
    *p = node;
    
    rb_insert_fixup(tree, node);
    tree->size++;
    
    return INFRA_OK;
}

struct infra_rb_node* infra_rbtree_find(struct infra_rb_tree* tree, struct infra_rb_node* key,
                                       int (*cmp)(struct infra_rb_node*, struct infra_rb_node*)) {
    struct infra_rb_node* node = tree->root;
    
    while (node) {
        int res = cmp(key, node);
        if (res < 0)
            node = node->left;
        else if (res > 0)
            node = node->right;
        else
            return node;
    }
    
    return NULL;
}

size_t infra_rbtree_size(struct infra_rb_tree* tree) {
    return tree->size;
}
