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
struct infra_queue {
    struct infra_list list;
    size_t size;
};

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
struct rb_node {
    struct rb_node* parent;
    struct rb_node* left;
    struct rb_node* right;
    int color;  // 0 = black, 1 = red
    void* key;
    void* value;
};

struct rb_tree {
    struct rb_node* root;
    size_t size;
    int (*compare)(const void*, const void*);
};

int infra_rbtree_init(struct rb_tree* tree, int (*compare)(const void*, const void*)) {
    if (!tree || !compare) {
        return INFRA_ERR_INVALID;
    }
    tree->root = NULL;
    tree->size = 0;
    tree->compare = compare;
    return INFRA_OK;
}

// ... additional rb-tree implementation functions would go here ...