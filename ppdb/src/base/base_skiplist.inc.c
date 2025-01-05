#include <cosmopolitan.h>

#define MAX_SKIPLIST_LEVEL 32
#define SKIPLIST_P 0.25

typedef struct ppdb_node_s {
    ppdb_key_t key;
    ppdb_value_t value;
    int height;
    struct ppdb_node_s* next[0];  // flexible array member
} ppdb_node_t;

static int random_level() {
    int level = 1;
    while ((rand() / (double)RAND_MAX) < SKIPLIST_P && level < MAX_SKIPLIST_LEVEL) {
        level++;
    }
    return level;
}

ppdb_node_t* node_create(ppdb_base_t* base, ppdb_key_t* key, ppdb_value_t* value, int height) {
    size_t node_size = sizeof(ppdb_node_t) + height * sizeof(ppdb_node_t*);
    ppdb_node_t* node = PPDB_ALIGNED_ALLOC(node_size);
    if (!node) return NULL;
    
    memset(node, 0, node_size);
    node->height = height;
    
    if (key) {
        node->key.size = key->size;
        node->key.data = PPDB_ALIGNED_ALLOC(key->size);
        if (!node->key.data) {
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
        memcpy(node->key.data, key->data, key->size);
    }
    
    if (value) {
        node->value.size = value->size;
        node->value.data = PPDB_ALIGNED_ALLOC(value->size);
        if (!node->value.data) {
            if (key) PPDB_ALIGNED_FREE(node->key.data);
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
        memcpy(node->value.data, value->data, value->size);
    }
    
    return node;
}

void node_destroy(ppdb_node_t* node) {
    if (!node) return;
    if (node->key.data) PPDB_ALIGNED_FREE(node->key.data);
    if (node->value.data) PPDB_ALIGNED_FREE(node->value.data);
    PPDB_ALIGNED_FREE(node);
}

int node_get_height(ppdb_node_t* node) {
    return node ? node->height : 0;
}

ppdb_node_t* skiplist_find(ppdb_node_t* head, ppdb_key_t* key, ppdb_node_t** update) {
    ppdb_node_t* current = head;
    
    for (int i = head->height - 1; i >= 0; i--) {
        while (current->next[i] && 
               memcmp(current->next[i]->key.data, key->data, 
                     MIN(current->next[i]->key.size, key->size)) < 0) {
            current = current->next[i];
        }
        if (update) update[i] = current;
    }
    
    current = current->next[0];
    
    if (current && current->key.size == key->size && 
        memcmp(current->key.data, key->data, key->size) == 0) {
        return current;
    }
    
    return NULL;
}

ppdb_error_t skiplist_insert(ppdb_base_t* base, ppdb_node_t* head, 
                            ppdb_key_t* key, ppdb_value_t* value) {
    ppdb_node_t* update[MAX_SKIPLIST_LEVEL];
    ppdb_node_t* node = skiplist_find(head, key, update);
    
    if (node) {
        // Update existing node
        uint8_t* new_value = PPDB_ALIGNED_ALLOC(value->size);
        if (!new_value) return PPDB_ERROR_OOM;
        
        memcpy(new_value, value->data, value->size);
        PPDB_ALIGNED_FREE(node->value.data);
        node->value.data = new_value;
        node->value.size = value->size;
        return PPDB_OK;
    }
    
    int level = random_level();
    node = node_create(base, key, value, level);
    if (!node) return PPDB_ERROR_OOM;
    
    for (int i = 0; i < level; i++) {
        node->next[i] = update[i]->next[i];
        update[i]->next[i] = node;
    }
    
    return PPDB_OK;
}

ppdb_error_t skiplist_delete(ppdb_node_t* head, ppdb_key_t* key) {
    ppdb_node_t* update[MAX_SKIPLIST_LEVEL];
    ppdb_node_t* node = skiplist_find(head, key, update);
    
    if (!node) return PPDB_ERROR_NOT_FOUND;
    
    for (int i = 0; i < node->height; i++) {
        update[i]->next[i] = node->next[i];
    }
    
    node_destroy(node);
    return PPDB_OK;
} 