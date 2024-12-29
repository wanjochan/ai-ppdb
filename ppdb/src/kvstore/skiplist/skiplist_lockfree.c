#include <cosmopolitan.h>
#include "ppdb/skiplist_lockfree.h"
#include "ppdb/logger.h"
#include "ppdb/error.h"
#include "ppdb/ref_count.h"

#define P 0.25

// Forward declarations
static void destroy_node(skiplist_node_t* node);
static uint32_t random_level(void);
static int compare_keys(const uint8_t* key1, size_t key1_len,
                       const uint8_t* key2, size_t key2_len);

// Generate random level (thread-safe)
static uint32_t random_level(void) {
    uint32_t level = 1;
    while ((rand() & 0xFFFF) < (P * 0xFFFF) && level < MAX_LEVEL) {
        level++;
    }
    ppdb_log_debug("Generated random level: %u", level);
    return level;
}

// Create a node with given level
static skiplist_node_t* create_node(uint32_t level,
                                  const uint8_t* key, size_t key_len,
                                  const uint8_t* value, size_t value_len) {
    size_t node_size = sizeof(skiplist_node_t) + level * sizeof(_Atomic(skiplist_node_t*));
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
    memcpy(node->key, key, key_len);
    node->key_len = key_len;

    // Allocate and copy value
    node->value = malloc(value_len);
    if (!node->value) {
        ppdb_log_error("Failed to allocate value buffer");
        free(node->key);
        free(node);
        return NULL;
    }
    memcpy(node->value, value, value_len);
    node->value_len = value_len;

    // Initialize other fields
    node->level = level;
    atomic_init(&node->state, NODE_VALID);
    for (uint32_t i = 0; i < level; i++) {
        atomic_init(&node->next[i], NULL);
    }

    // Create reference counter
    node->ref_count = ref_count_create(node, (ref_count_free_fn)free);
    if (!node->ref_count) {
        ppdb_log_error("Failed to create reference counter");
        free(node->value);
        free(node->key);
        free(node);
        return NULL;
    }

    ppdb_log_debug("Created skiplist node: level=%u, key_len=%zu, value_len=%zu",
                   level, key_len, value_len);
    return node;
}

// Compare two keys
static int compare_keys(const uint8_t* key1, size_t key1_len,
                       const uint8_t* key2, size_t key2_len) {
    size_t min_len = key1_len < key2_len ? key1_len : key2_len;
    int result = memcmp(key1, key2, min_len);
    if (result != 0) return result;
    return (int)key1_len - (int)key2_len;
}

// Create lock-free skip list
atomic_skiplist_t* atomic_skiplist_create(void) {
    atomic_skiplist_t* list = malloc(sizeof(atomic_skiplist_t));
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
    list->max_level = MAX_LEVEL;

    ppdb_log_info("Created lock-free skiplist");
    return list;
}

// Destroy lock-free skip list node
static void destroy_node(skiplist_node_t* node) {
    if (node) {
        if (node->key) {
            memset(node->key, 0, node->key_len);  // 清零键
            free(node->key);
        }
        if (node->value) {
            memset(node->value, 0, node->value_len);  // 清零值
            free(node->value);
        }
        memset(node, 0, sizeof(skiplist_node_t) + (node->level - 1) * sizeof(skiplist_node_t*));  // 清零节点
        free(node);
    }
}

// Destroy lock-free skip list
void atomic_skiplist_destroy(atomic_skiplist_t* list) {
    if (!list) return;

    skiplist_node_t* current = list->head;
    while (current) {
        skiplist_node_t* next = atomic_load(&current->next[0]);
        destroy_node(current);
        current = next;
    }

    memset(list, 0, sizeof(atomic_skiplist_t));  // 清零跳表结构
    free(list);
}

// Get value corresponding to key
ppdb_error_t atomic_skiplist_get(atomic_skiplist_t* list,
                                const uint8_t* key, size_t key_len,
                                uint8_t* value, size_t* value_len) {
    if (!list || !key || !value_len) {
        ppdb_log_error("Invalid parameters in skiplist_get");
        return PPDB_ERR_INVALID_ARG;
    }

    skiplist_node_t* current = list->head;
    
    // Start searching from the highest level
    for (int32_t level = (int32_t)list->max_level - 1; level >= 0; level--) {
        skiplist_node_t* next;
        do {
            next = atomic_load_explicit(&current->next[level], memory_order_acquire);
            if (!next) break;

            int cmp = compare_keys(key, key_len, next->key, next->key_len);
            if (cmp < 0) break;
            if (cmp == 0 && atomic_load(&next->state) == NODE_VALID) {
                // If node is found, first return value size
                if (!value) {
                    *value_len = next->value_len;
                    return PPDB_OK;
                }

                if (*value_len < next->value_len) {
                    *value_len = next->value_len;
                    ppdb_log_error("Buffer too small for value");
                    return PPDB_ERR_BUFFER_TOO_SMALL;
                }

                // Copy value
                memcpy(value, next->value, next->value_len);
                *value_len = next->value_len;
                ppdb_log_debug("Retrieved key from skiplist");
                return PPDB_OK;
            }
            current = next;
        } while (1);
    }

    ppdb_log_debug("Key not found in skiplist");
    return PPDB_ERR_NOT_FOUND;
}

// Insert/update key-value pair
ppdb_error_t atomic_skiplist_put(atomic_skiplist_t* list,
                                const uint8_t* key, size_t key_len,
                                const uint8_t* value, size_t value_len) {
    if (!list || !key || !value || key_len == 0 || value_len == 0) {
        ppdb_log_error("Invalid parameters in skiplist_put");
        return PPDB_ERR_INVALID_ARG;
    }

    skiplist_node_t* update[MAX_LEVEL];
    skiplist_node_t* current = list->head;
    skiplist_node_t* found = NULL;

    // Find position at each level
    for (int32_t i = (int32_t)list->max_level - 1; i >= 0; i--) {
        skiplist_node_t* next;
        do {
            next = atomic_load_explicit(&current->next[i], memory_order_acquire);
            if (!next || compare_keys(key, key_len, next->key, next->key_len) < 0) {
                update[i] = current;
                break;
            }
            if (compare_keys(key, key_len, next->key, next->key_len) == 0) {
                if (atomic_load(&next->state) == NODE_VALID) {
                    found = next;
                    update[i] = current;
                    break;
                }
            }
            current = next;
        } while (1);
    }

    // Update existing node
    if (found) {
        void* new_value = malloc(value_len);
        if (!new_value) {
            ppdb_log_error("Failed to allocate new value buffer");
            return PPDB_ERR_NO_MEMORY;
        }

        memcpy(new_value, value, value_len);
        void* old_value = found->value;
        found->value = new_value;
        found->value_len = value_len;
        free(old_value);

        ppdb_log_debug("Updated existing key in skiplist");
        return PPDB_OK;
    }

    // Create new node
    uint32_t level = random_level();
    skiplist_node_t* new_node = create_node(level, key, key_len, value, value_len);
    if (!new_node) {
        ppdb_log_error("Failed to create new node");
        return PPDB_ERR_NO_MEMORY;
    }

    // Insert new node
    for (uint32_t i = 0; i < level; i++) {
        do {
            skiplist_node_t* next = atomic_load_explicit(&update[i]->next[i], memory_order_acquire);
            atomic_store_explicit(&new_node->next[i], next, memory_order_release);
        } while (!atomic_compare_exchange_weak_explicit(
            &update[i]->next[i],
            &new_node->next[i],
            new_node,
            memory_order_release,
            memory_order_relaxed));
    }

    atomic_fetch_add_explicit(&list->size, 1, memory_order_relaxed);
    ppdb_log_debug("Inserted new key in skiplist");
    return PPDB_OK;
}

// Delete key-value pair
ppdb_error_t atomic_skiplist_delete(atomic_skiplist_t* list,
                                const uint8_t* key, size_t key_len) {
    if (!list || !key || key_len == 0) {
        ppdb_log_error("Invalid parameters in skiplist_delete");
        return PPDB_ERR_INVALID_ARG;
    }

    skiplist_node_t* update[MAX_LEVEL];
    skiplist_node_t* current = list->head;
    skiplist_node_t* target = NULL;

    // Find node to delete with retries
    int retries = 0;
    const int max_retries = 3;
    do {
        // Find node to delete
        for (int32_t i = (int32_t)list->max_level - 1; i >= 0; i--) {
            skiplist_node_t* next;
            do {
                next = atomic_load_explicit(&current->next[i], memory_order_acquire);
                if (!next || compare_keys(key, key_len, next->key, next->key_len) < 0) {
                    update[i] = current;
                    break;
                }
                if (compare_keys(key, key_len, next->key, next->key_len) == 0) {
                    target = next;
                    update[i] = current;
                    break;
                }
                current = next;
            } while (1);
        }

        if (!target) {
            ppdb_log_debug("Key not found in skiplist");
            return PPDB_ERR_NOT_FOUND;
        }

        // Mark node as deleted
        uint32_t expected = NODE_VALID;
        if (atomic_compare_exchange_strong_explicit(
                &target->state,
                &expected,
                NODE_DELETED,
                memory_order_release,
                memory_order_relaxed)) {
            break;
        }

        // If node is already deleted, retry
        if (expected == NODE_DELETED) {
            if (retries++ >= max_retries) {
                ppdb_log_error("Max retries reached for deletion");
                return PPDB_ERR_INTERNAL;
            }
            current = list->head;
            target = NULL;
            continue;
        }
    } while (retries < max_retries);

    // Update pointers with retries
    for (uint32_t i = 0; i < target->level; i++) {
        skiplist_node_t* next;
        int level_retries = 0;
        do {
            next = atomic_load_explicit(&target->next[i], memory_order_acquire);
            if (atomic_compare_exchange_weak_explicit(
                    &update[i]->next[i],
                    &target,
                    next,
                    memory_order_release,
                    memory_order_relaxed)) {
                break;
            }
            if (level_retries++ >= max_retries) {
                ppdb_log_error("Max retries reached for pointer update at level %d", i);
                return PPDB_ERR_INTERNAL;
            }
        } while (1);
    }

    atomic_fetch_sub_explicit(&list->size, 1, memory_order_release);
    ref_count_dec(target->ref_count);  // This will free the node when count reaches 0

    // Verify deletion
    current = list->head;
    for (int32_t i = (int32_t)list->max_level - 1; i >= 0; i--) {
        skiplist_node_t* next;
        do {
            next = atomic_load_explicit(&current->next[i], memory_order_acquire);
            if (!next || compare_keys(key, key_len, next->key, next->key_len) < 0) {
                break;
            }
            if (compare_keys(key, key_len, next->key, next->key_len) == 0 &&
                atomic_load_explicit(&next->state, memory_order_acquire) == NODE_VALID) {
                ppdb_log_error("Key still exists after deletion in skiplist");
                return PPDB_ERR_INTERNAL;
            }
            current = next;
        } while (1);
    }

    ppdb_log_debug("Deleted key from skiplist");
    return PPDB_OK;
}

// Get skip list size
size_t atomic_skiplist_size(atomic_skiplist_t* list) {
    if (!list) return 0;
    return atomic_load_explicit(&list->size, memory_order_relaxed);
}

// Clear skip list
void atomic_skiplist_clear(atomic_skiplist_t* list) {
    if (!list) return;

    skiplist_node_t* current = atomic_load_explicit(&list->head->next[0], memory_order_acquire);
    while (current) {
        skiplist_node_t* next = atomic_load_explicit(&current->next[0], memory_order_acquire);
        uint32_t expected = NODE_VALID;
        if (atomic_compare_exchange_strong_explicit(
                &current->state,
                &expected,
                NODE_DELETED,
                memory_order_release,
                memory_order_relaxed)) {
            ref_count_dec(current->ref_count);
        }
        current = next;
    }

    atomic_store_explicit(&list->size, 0, memory_order_release);
    ppdb_log_info("Cleared skiplist");
}

// Traverse skip list with visitor pattern
void atomic_skiplist_foreach(atomic_skiplist_t* list,
                           skiplist_visitor_t visitor,
                           void* ctx) {
    if (!list || !visitor) {
        ppdb_log_error("Invalid parameters in skiplist_foreach");
        return;
    }

    skiplist_node_t* current = atomic_load_explicit(&list->head->next[0], memory_order_acquire);
    while (current) {
        if (atomic_load(&current->state) == NODE_VALID) {
            if (!visitor(current->key, current->key_len,
                        current->value, current->value_len, ctx)) {
                break;
            }
        }
        current = atomic_load_explicit(&current->next[0], memory_order_acquire);
    }
}

// Create iterator
atomic_skiplist_iterator_t* atomic_skiplist_iterator_create(atomic_skiplist_t* list) {
    if (!list) {
        ppdb_log_error("Invalid list parameter in iterator_create");
        return NULL;
    }

    atomic_skiplist_iterator_t* iter = malloc(sizeof(atomic_skiplist_iterator_t));
    if (!iter) {
        ppdb_log_error("Failed to allocate iterator");
        return NULL;
    }

    iter->list = list;
    iter->current = atomic_load_explicit(&list->head->next[0], memory_order_acquire);
    
    // Skip deleted nodes at initialization
    while (iter->current && atomic_load(&iter->current->state) == NODE_DELETED) {
        iter->current = atomic_load_explicit(&iter->current->next[0], memory_order_acquire);
    }

    // Create reference counter for the iterator
    iter->ref_count = ref_count_create(iter, (ref_count_free_fn)free);
    if (!iter->ref_count) {
        ppdb_log_error("Failed to create reference counter for iterator");
        free(iter);
        return NULL;
    }

    // Increment reference count for current node if valid
    if (iter->current) {
        ref_count_inc(iter->current->ref_count);
    }

    ppdb_log_debug("Created skiplist iterator");
    return iter;
}

// Destroy iterator
void atomic_skiplist_iterator_destroy(atomic_skiplist_iterator_t* iter) {
    if (!iter) return;

    // Decrement reference count for current node if valid
    if (iter->current) {
        ref_count_dec(iter->current->ref_count);
    }

    // Decrement reference count for iterator
    ref_count_dec(iter->ref_count);
    ppdb_log_debug("Destroyed skiplist iterator");
}

// Check if iterator is valid
bool atomic_skiplist_iterator_valid(atomic_skiplist_iterator_t* iter) {
    if (!iter) return false;
    return iter->current != NULL && atomic_load(&iter->current->state) == NODE_VALID;
}

// Move iterator to next valid node and get key-value pair
bool atomic_skiplist_iterator_next(atomic_skiplist_iterator_t* iter,
                                uint8_t** key, size_t* key_size,
                                uint8_t** value, size_t* value_size) {
    if (!iter || !key || !key_size || !value || !value_size) {
        ppdb_log_error("Invalid parameters in iterator_next");
        return false;
    }

    // Skip deleted nodes
    while (iter->current && atomic_load(&iter->current->state) != NODE_VALID) {
        skiplist_node_t* next = atomic_load_explicit(&iter->current->next[0], memory_order_acquire);
        ref_count_dec(iter->current->ref_count);
        iter->current = next;
        if (iter->current) {
            ref_count_inc(iter->current->ref_count);
        }
    }

    if (!iter->current) {
        return false;  // Already reached the end
    }

    // Get current key-value pair
    *key = iter->current->key;
    *key_size = iter->current->key_len;
    *value = iter->current->value;
    *value_size = iter->current->value_len;

    // Move to next node
    skiplist_node_t* next = atomic_load_explicit(&iter->current->next[0], memory_order_acquire);
    ref_count_dec(iter->current->ref_count);
    iter->current = next;
    if (iter->current) {
        ref_count_inc(iter->current->ref_count);
    }

    ppdb_log_debug("Advanced skiplist iterator");
    return true;
}
