#include <cosmopolitan.h>
#include "ppdb/skiplist_mutex.h"
#include "ppdb/logger.h"
#include "ppdb/error.h"

#define MAX_LEVEL 32
#define P 0.25

// Skip list node structure
typedef struct skiplist_node_t {
    uint8_t* key;
    size_t key_len;
    uint8_t* value;
    size_t value_len;
    uint32_t level;
    struct skiplist_node_t* next[1];  // Flexible array
} skiplist_node_t;

// Skip list structure with mutex
typedef struct skiplist_t {
    skiplist_node_t* head;        // Head node
    atomic_size_t size;           // Number of nodes
    uint32_t max_level;           // Maximum level
    pthread_mutex_t mutex;        // Mutex for thread safety
} skiplist_t;

// Iterator structure
typedef struct skiplist_iterator_t {
    skiplist_t* list;            // Associated skip list
    skiplist_node_t* current;    // Current node
    pthread_mutex_t* mutex;      // Reference to list mutex
} skiplist_iterator_t;

// Create a node with given level
static skiplist_node_t* create_node(uint32_t level,
                                  const uint8_t* key, size_t key_len,
                                  const uint8_t* value, size_t value_len) {
    size_t node_size = sizeof(skiplist_node_t) + level * sizeof(skiplist_node_t*);
    skiplist_node_t* node = malloc(node_size);
    if (!node) {
        ppdb_log_error("Failed to allocate skiplist node");
        return NULL;
    }

    // Allocate and copy key
    node->key = malloc(key_len);
    if (!node->key) {
        ppdb_log_error("Failed to allocate key buffer");
        free(node);
        return NULL;
    }

    // Allocate and copy value
    node->value = malloc(value_len);
    if (!node->value) {
        ppdb_log_error("Failed to allocate value buffer");
        free(node->key);
        free(node);
        return NULL;
    }

    memcpy(node->key, key, key_len);
    memcpy(node->value, value, value_len);
    node->key_len = key_len;
    node->value_len = value_len;
    node->level = level;

    // Initialize next pointers
    for (uint32_t i = 0; i < level; i++) {
        node->next[i] = NULL;
    }

    ppdb_log_debug("Created skiplist node: level=%u, key_len=%zu, value_len=%zu",
                   level, key_len, value_len);
    return node;
}

// Destroy a node
static void destroy_node(skiplist_node_t* node) {
    if (!node) return;
    free(node->key);
    free(node->value);
    free(node);
    ppdb_log_debug("Destroyed skiplist node");
}

// Generate random level (thread-safe)
static uint32_t random_level(void) {
    uint32_t level = 1;
    while ((rand() & 0xFFFF) < (P * 0xFFFF) && level < MAX_LEVEL) {
        level++;
    }
    ppdb_log_debug("Generated random level: %u", level);
    return level;
}

// Compare two keys
static int compare_keys(const uint8_t* key1, size_t key1_len,
                       const uint8_t* key2, size_t key2_len) {
    size_t min_len = key1_len < key2_len ? key1_len : key2_len;
    int result = memcmp(key1, key2, min_len);
    if (result != 0) return result;
    return (int)key1_len - (int)key2_len;
}

// Create skip list
skiplist_t* skiplist_create(void) {
    skiplist_t* list = malloc(sizeof(skiplist_t));
    if (!list) {
        ppdb_log_error("Failed to allocate skiplist");
        return NULL;
    }

    // Create head node
    list->head = create_node(MAX_LEVEL, (uint8_t*)"", 0, (uint8_t*)"", 0);
    if (!list->head) {
        ppdb_log_error("Failed to create head node");
        free(list);
        return NULL;
    }

    atomic_init(&list->size, 0);
    list->max_level = 1;

    // Initialize mutex
    if (pthread_mutex_init(&list->mutex, NULL) != 0) {
        ppdb_log_error("Failed to initialize mutex");
        destroy_node(list->head);
        free(list);
        return NULL;
    }

    ppdb_log_info("Created skiplist");
    return list;
}

// Destroy skip list
void skiplist_destroy(skiplist_t* list) {
    if (!list) return;

    pthread_mutex_lock(&list->mutex);

    // Free all nodes
    skiplist_node_t* node = list->head;
    while (node) {
        skiplist_node_t* next = node->next[0];
        destroy_node(node);
        node = next;
    }

    pthread_mutex_unlock(&list->mutex);

    // Free mutex and list structure
    pthread_mutex_destroy(&list->mutex);
    free(list);

    ppdb_log_info("Destroyed skiplist");
}

// Insert/update key-value pair
ppdb_error_t skiplist_put(skiplist_t* list,
                         const uint8_t* key, size_t key_len,
                         const uint8_t* value, size_t value_len) {
    if (!list || !key || !value || key_len == 0 || value_len == 0) {
        ppdb_log_error("Invalid parameters in skiplist_put");
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&list->mutex);

    // Find position
    skiplist_node_t* update[MAX_LEVEL];
    skiplist_node_t* current = list->head;

    for (uint32_t i = list->max_level - 1; i < list->max_level; i--) {
        while (current->next[i] &&
               compare_keys(current->next[i]->key,
                          current->next[i]->key_len,
                          key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    current = current->next[0];

    // Update existing key
    if (current && compare_keys(current->key, current->key_len,
                              key, key_len) == 0) {
        uint8_t* new_value = malloc(value_len);
        if (!new_value) {
            ppdb_log_error("Failed to allocate new value buffer");
            pthread_mutex_unlock(&list->mutex);
            return PPDB_ERR_NO_MEMORY;
        }

        memcpy(new_value, value, value_len);
        
        // Save old value pointer before updating new value
        uint8_t* old_value = current->value;
        current->value = new_value;
        current->value_len = value_len;
        
        // Finally free old value
        free(old_value);
        
        ppdb_log_debug("Updated existing key in skiplist");
        pthread_mutex_unlock(&list->mutex);
        return PPDB_OK;
    }

    // Insert new node
    uint32_t new_level = random_level();
    skiplist_node_t* new_node = create_node(new_level, key, key_len,
                                          value, value_len);
    if (!new_node) {
        ppdb_log_error("Failed to create new node");
        pthread_mutex_unlock(&list->mutex);
        return PPDB_ERR_NO_MEMORY;
    }

    if (new_level > list->max_level) {
        for (uint32_t i = list->max_level; i < new_level; i++) {
            update[i] = list->head;
        }
        list->max_level = new_level;
    }

    for (uint32_t i = 0; i < new_level; i++) {
        new_node->next[i] = update[i]->next[i];
        update[i]->next[i] = new_node;
    }

    atomic_fetch_add_explicit(&list->size, 1, memory_order_relaxed);
    ppdb_log_debug("Inserted new key in skiplist");
    pthread_mutex_unlock(&list->mutex);
    return PPDB_OK;
}

// Get value corresponding to key
ppdb_error_t skiplist_get(skiplist_t* list,
                         const uint8_t* key, size_t key_len,
                         uint8_t* value, size_t* value_len) {
    if (!list || !key || !value_len) {
        ppdb_log_error("Invalid parameters in skiplist_get");
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&list->mutex);

    skiplist_node_t* current = list->head;
    skiplist_node_t* target = NULL;
    
    // Start searching from the highest level
    for (int32_t level = (int32_t)list->max_level - 1; level >= 0; level--) {
        while (current->next[level] &&
               compare_keys(current->next[level]->key,
                          current->next[level]->key_len,
                          key, key_len) < 0) {
            current = current->next[level];
        }
        if (current->next[level] &&
            compare_keys(current->next[level]->key,
                       current->next[level]->key_len,
                       key, key_len) == 0) {
            target = current->next[level];
            break;
        }
    }

    // If node is not found
    if (!target) {
        ppdb_log_debug("Key not found in skiplist");
        pthread_mutex_unlock(&list->mutex);
        return PPDB_ERR_NOT_FOUND;
    }

    // If node is found, first return value size
    if (!value) {
        *value_len = target->value_len;
        pthread_mutex_unlock(&list->mutex);
        return PPDB_OK;
    }

    if (*value_len < target->value_len) {
        *value_len = target->value_len;
        ppdb_log_error("Buffer too small for value");
        pthread_mutex_unlock(&list->mutex);
        return PPDB_ERR_BUFFER_TOO_SMALL;
    }

    // Copy value
    memcpy(value, target->value, target->value_len);
    *value_len = target->value_len;

    ppdb_log_debug("Retrieved key from skiplist");
    pthread_mutex_unlock(&list->mutex);
    return PPDB_OK;
}

// Delete key-value pair
ppdb_error_t skiplist_delete(skiplist_t* list,
                            const uint8_t* key, size_t key_len) {
    if (!list || !key || key_len == 0) {
        ppdb_log_error("Invalid parameters in skiplist_delete");
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&list->mutex);

    skiplist_node_t* update[MAX_LEVEL];
    skiplist_node_t* current = list->head;

    for (int32_t i = (int32_t)list->max_level - 1; i >= 0; i--) {
        while (current->next[i] &&
               compare_keys(current->next[i]->key,
                          current->next[i]->key_len,
                          key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    current = current->next[0];
    if (!current || compare_keys(current->key, current->key_len,
                               key, key_len) != 0) {
        ppdb_log_debug("Key not found for deletion");
        pthread_mutex_unlock(&list->mutex);
        return PPDB_ERR_NOT_FOUND;
    }

    for (uint32_t i = 0; i < list->max_level; i++) {
        if (update[i]->next[i] != current) {
            break;
        }
        update[i]->next[i] = current->next[i];
    }

    destroy_node(current);

    while (list->max_level > 1 && !list->head->next[list->max_level - 1]) {
        list->max_level--;
    }

    atomic_fetch_sub_explicit(&list->size, 1, memory_order_relaxed);
    ppdb_log_debug("Deleted key from skiplist");
    pthread_mutex_unlock(&list->mutex);
    return PPDB_OK;
}

// Get skip list size
size_t skiplist_size(skiplist_t* list) {
    if (!list) return 0;
    return atomic_load_explicit(&list->size, memory_order_relaxed);
}

// Create iterator
skiplist_iterator_t* skiplist_iterator_create(skiplist_t* list) {
    if (!list) {
        ppdb_log_error("Invalid list parameter in iterator_create");
        return NULL;
    }

    skiplist_iterator_t* iter = malloc(sizeof(skiplist_iterator_t));
    if (!iter) {
        ppdb_log_error("Failed to allocate iterator");
        return NULL;
    }

    iter->list = list;
    iter->current = list->head->next[0];  // Start from first actual node
    iter->mutex = &list->mutex;

    ppdb_log_debug("Created skiplist iterator");
    return iter;
}

// Destroy iterator
void skiplist_iterator_destroy(skiplist_iterator_t* iter) {
    if (!iter) return;
    ppdb_log_debug("Destroyed skiplist iterator");
    free(iter);
}

// Get next key-value pair
bool skiplist_iterator_next(skiplist_iterator_t* iter,
                          uint8_t** key, size_t* key_size,
                          uint8_t** value, size_t* value_size) {
    if (!iter || !key || !key_size || !value || !value_size) {
        ppdb_log_error("Invalid parameters in iterator_next");
        return false;
    }

    pthread_mutex_lock(iter->mutex);

    if (!iter->current) {
        pthread_mutex_unlock(iter->mutex);
        return false;  // Already reached the end
    }

    *key = iter->current->key;
    *key_size = iter->current->key_len;
    *value = iter->current->value;
    *value_size = iter->current->value_len;

    iter->current = iter->current->next[0];  // Move to the next node

    pthread_mutex_unlock(iter->mutex);
    return true;
}