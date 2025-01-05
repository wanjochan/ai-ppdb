#ifndef PPDB_BASE_SKIPLIST_INC_C
#define PPDB_BASE_SKIPLIST_INC_C

#define SKIPLIST_MAX_LEVEL 32

typedef struct skiplist_node {
    ppdb_data_t key;
    ppdb_data_t value;
    struct skiplist_node* next[1];  // Variable length array
} skiplist_node_t;

typedef struct skiplist {
    int level;
    size_t size;
    skiplist_node_t* header;
    ppdb_core_mutex_t* mutex;
} skiplist_t;

static int random_level(void) {
    int level = 1;
    while ((rand() & 0xFFFF) < (0xFFFF >> 1) && level < SKIPLIST_MAX_LEVEL) {
        level++;
    }
    return level;
}

ppdb_error_t skiplist_create(skiplist_t** list) {
    if (!list) return PPDB_ERR_NULL_POINTER;

    // Allocate skiplist
    skiplist_t* sl = (skiplist_t*)ppdb_aligned_alloc(sizeof(skiplist_t));
    if (!sl) return PPDB_ERR_OUT_OF_MEMORY;

    // Create mutex
    ppdb_error_t err = ppdb_core_mutex_create(&sl->mutex);
    if (err != PPDB_OK) {
        ppdb_aligned_free(sl);
        return err;
    }

    // Allocate header node
    size_t node_size = sizeof(skiplist_node_t) + (SKIPLIST_MAX_LEVEL - 1) * sizeof(skiplist_node_t*);
    sl->header = (skiplist_node_t*)ppdb_aligned_alloc(node_size);
    if (!sl->header) {
        ppdb_core_mutex_destroy(sl->mutex);
        ppdb_aligned_free(sl);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // Initialize header
    memset(sl->header, 0, node_size);
    sl->level = 1;
    sl->size = 0;

    *list = sl;
    return PPDB_OK;
}

void skiplist_destroy(skiplist_t* list) {
    if (!list) return;

    // Lock list
    ppdb_core_mutex_lock(list->mutex);

    // Free all nodes
    skiplist_node_t* node = list->header->next[0];
    while (node) {
        skiplist_node_t* next = node->next[0];
        ppdb_data_destroy(&node->key);
        ppdb_data_destroy(&node->value);
        ppdb_aligned_free(node);
        node = next;
    }

    // Free header and list
    ppdb_aligned_free(list->header);
    ppdb_core_mutex_unlock(list->mutex);
    ppdb_core_mutex_destroy(list->mutex);
    ppdb_aligned_free(list);
}

ppdb_error_t skiplist_insert(skiplist_t* list, const ppdb_data_t* key, const ppdb_data_t* value) {
    if (!list || !key || !value) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_lock(list->mutex);

    // Find position to insert
    skiplist_node_t* update[SKIPLIST_MAX_LEVEL];
    skiplist_node_t* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->next[i] && memcmp(x->next[i]->key.inline_data, key->inline_data, key->size) < 0) {
            x = x->next[i];
        }
        update[i] = x;
    }
    x = x->next[0];

    // Key exists, update value
    if (x && memcmp(x->key.inline_data, key->inline_data, key->size) == 0) {
        ppdb_data_destroy(&x->value);
        ppdb_error_t err = ppdb_data_create(&x->value, value->inline_data, value->size);
        ppdb_core_mutex_unlock(list->mutex);
        return err;
    }

    // Create new node
    int level = random_level();
    if (level > list->level) {
        for (int i = list->level; i < level; i++) {
            update[i] = list->header;
        }
        list->level = level;
    }

    size_t node_size = sizeof(skiplist_node_t) + (level - 1) * sizeof(skiplist_node_t*);
    skiplist_node_t* new_node = (skiplist_node_t*)ppdb_aligned_alloc(node_size);
    if (!new_node) {
        ppdb_core_mutex_unlock(list->mutex);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // Initialize new node
    memset(new_node, 0, node_size);
    ppdb_error_t err = ppdb_data_create(&new_node->key, key->inline_data, key->size);
    if (err != PPDB_OK) {
        ppdb_aligned_free(new_node);
        ppdb_core_mutex_unlock(list->mutex);
        return err;
    }

    err = ppdb_data_create(&new_node->value, value->inline_data, value->size);
    if (err != PPDB_OK) {
        ppdb_data_destroy(&new_node->key);
        ppdb_aligned_free(new_node);
        ppdb_core_mutex_unlock(list->mutex);
        return err;
    }

    // Insert node
    for (int i = 0; i < level; i++) {
        new_node->next[i] = update[i]->next[i];
        update[i]->next[i] = new_node;
    }

    list->size++;
    ppdb_core_mutex_unlock(list->mutex);
    return PPDB_OK;
}

ppdb_error_t skiplist_delete(skiplist_t* list, const ppdb_data_t* key) {
    if (!list || !key) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_lock(list->mutex);

    // Find node to delete
    skiplist_node_t* update[SKIPLIST_MAX_LEVEL];
    skiplist_node_t* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->next[i] && memcmp(x->next[i]->key.inline_data, key->inline_data, key->size) < 0) {
            x = x->next[i];
        }
        update[i] = x;
    }
    x = x->next[0];

    // Key not found
    if (!x || memcmp(x->key.inline_data, key->inline_data, key->size) != 0) {
        ppdb_core_mutex_unlock(list->mutex);
        return PPDB_ERR_NOT_FOUND;
    }

    // Remove node from all levels
    for (int i = 0; i < list->level; i++) {
        if (update[i]->next[i] != x) break;
        update[i]->next[i] = x->next[i];
    }

    // Update list level
    while (list->level > 1 && list->header->next[list->level - 1] == NULL) {
        list->level--;
    }

    // Free node
    ppdb_data_destroy(&x->key);
    ppdb_data_destroy(&x->value);
    ppdb_aligned_free(x);

    list->size--;
    ppdb_core_mutex_unlock(list->mutex);
    return PPDB_OK;
}

ppdb_error_t skiplist_find(skiplist_t* list, const ppdb_data_t* key, ppdb_data_t* value) {
    if (!list || !key || !value) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_lock(list->mutex);

    // Find node
    skiplist_node_t* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->next[i] && memcmp(x->next[i]->key.inline_data, key->inline_data, key->size) < 0) {
            x = x->next[i];
        }
    }
    x = x->next[0];

    ppdb_error_t err;
    if (x && memcmp(x->key.inline_data, key->inline_data, key->size) == 0) {
        err = ppdb_data_create(value, x->value.inline_data, x->value.size);
    } else {
        err = PPDB_ERR_NOT_FOUND;
    }

    ppdb_core_mutex_unlock(list->mutex);
    return err;
}

#endif // PPDB_BASE_SKIPLIST_INC_C 