#include <cosmopolitan.h>
#include "ppdb/skiplist_mutex.h"
#include "ppdb/error.h"
#include "ppdb/logger.h"

// Complete structure definitions
struct skiplist_t {
    skiplist_node_t* head;        // Head node
    atomic_size_t size;           // Number of nodes
    uint32_t max_level;           // Maximum level
    pthread_mutex_t mutex;        // Mutex for thread safety
};

struct skiplist_iterator_t {
    skiplist_t* list;            // Associated skip list
    skiplist_node_t* current;    // Current node
    pthread_mutex_t* mutex;      // Reference to list mutex
};

// Forward declarations
static void destroy_node(skiplist_node_t* node);
static uint32_t random_level(void);
static int compare_keys(const uint8_t* key1, size_t key1_len,
                       const uint8_t* key2, size_t key2_len);

// Helper functions
static skiplist_node_t* create_node(uint32_t height,
                                  const uint8_t* key, size_t key_len,
                                  const uint8_t* value, size_t value_len) {
    if (height == 0 || height > MAX_LEVEL) {
        ppdb_log_error("Invalid node height: %u", height);
        return NULL;
    }

    // Calculate total size including the flexible array member
    size_t node_size = sizeof(skiplist_node_t) + height * sizeof(skiplist_node_t*);
    skiplist_node_t* node = NULL;
    uint8_t* key_buf = NULL;
    uint8_t* value_buf = NULL;

    // Pre-allocate buffers to avoid memory leaks in error paths
    if (key && key_len > 0) {
        key_buf = (uint8_t*)malloc(key_len);
        if (!key_buf) {
            ppdb_log_error("Failed to allocate key buffer");
            goto error;
        }
        memcpy(key_buf, key, key_len);
    }

    if (value && value_len > 0) {
        value_buf = (uint8_t*)malloc(value_len);
        if (!value_buf) {
            ppdb_log_error("Failed to allocate value buffer");
            goto error;
        }
        memcpy(value_buf, value, value_len);
    }

    // Allocate node after buffers are ready
    node = (skiplist_node_t*)calloc(1, node_size);
    if (!node) {
        ppdb_log_error("Failed to allocate skiplist node");
        goto error;
    }

    // Initialize node fields
    node->height = height;
    node->state = NODE_ACTIVE;
    node->key = key_buf;
    node->key_len = key_len;
    node->value = value_buf;
    node->value_len = value_len;

    // Initialize all next pointers to NULL
    for (uint32_t i = 0; i < height && i < MAX_LEVEL; i++) {
        node->next[i] = NULL;
    }

    ppdb_log_debug("Created skiplist node: height=%u, key_len=%zu, value_len=%zu",
                   height, key_len, value_len);
    return node;

error:
    // Clean up in error path
    if (key_buf) free(key_buf);
    if (value_buf) free(value_buf);
    if (node) {
        memset(node, 0, node_size);
        free(node);
    }
    return NULL;
}

static void destroy_node(skiplist_node_t* node) {
    if (!node) return;

    // 获取节点大小
    size_t node_size = sizeof(skiplist_node_t) + node->height * sizeof(skiplist_node_t*);

    // 标记节点为已删除，防止其他线程访问
    node->state = NODE_DELETED;

    // 清除next指针防止悬空指针
    for (uint32_t i = 0; i < node->height && i < MAX_LEVEL; i++) {
        node->next[i] = NULL;
    }

    // 安全释放key
    if (node->key) {
        memset(node->key, 0, node->key_len);  // 清零敏感数据
        free(node->key);
        node->key = NULL;
        node->key_len = 0;
    }

    // 安全释放value
    if (node->value) {
        memset(node->value, 0, node->value_len);  // 清零敏感数据
        free(node->value);
        node->value = NULL;
        node->value_len = 0;
    }

    // 清零高度
    node->height = 0;

    // 清零整个节点结构
    memset(node, 0, node_size);

    // 最后释放节点
    free(node);
}

static uint32_t random_level(void) {
    uint32_t level = 1;
    while ((rand() & 0xFFFF) < (0xFFFF >> 1) && level < MAX_LEVEL) {
        level++;
    }
    return level;
}

static int compare_keys(const uint8_t* key1, size_t key1_len,
                       const uint8_t* key2, size_t key2_len) {
    if (!key1 || !key2) return 0;  // Special case for head node
    size_t min_len = key1_len < key2_len ? key1_len : key2_len;
    int result = memcmp(key1, key2, min_len);
    if (result != 0) return result;
    if (key1_len < key2_len) return -1;
    if (key1_len > key2_len) return 1;
    return 0;
}

// Implementation of public functions
skiplist_t* skiplist_create(void) {
    // Allocate and zero initialize the skiplist structure
    skiplist_t* list = (skiplist_t*)calloc(1, sizeof(skiplist_t));
    if (!list) {
        ppdb_log_error("Failed to allocate skiplist");
        return NULL;
    }

    // Initialize fields first
    list->max_level = MAX_LEVEL;
    atomic_init(&list->size, 0);
    list->head = NULL;

    // Initialize mutex
    if (pthread_mutex_init(&list->mutex, NULL) != 0) {
        ppdb_log_error("Failed to initialize mutex");
        free(list);
        return NULL;
    }

    // Create head node with maximum height
    list->head = create_node(MAX_LEVEL, NULL, 0, NULL, 0);
    if (!list->head) {
        ppdb_log_error("Failed to create head node");
        pthread_mutex_destroy(&list->mutex);
        free(list);
        return NULL;
    }

    // Initialize all next pointers in head node to NULL
    for (uint32_t i = 0; i < MAX_LEVEL; i++) {
        list->head->next[i] = NULL;
    }

    // Set head node state
    list->head->state = NODE_ACTIVE;
    list->head->height = MAX_LEVEL;

    ppdb_log_debug("Created skiplist");
    return list;
}

void skiplist_destroy(skiplist_t* list) {
    if (!list) return;

    // 先加锁防止并发访问
    pthread_mutex_lock(&list->mutex);

    // 保存头节点指针并清空list结构
    skiplist_node_t* head = list->head;
    list->head = NULL;
    atomic_store(&list->size, 0);
    list->max_level = 0;

    // 解锁并销毁互斥锁
    pthread_mutex_unlock(&list->mutex);
    pthread_mutex_destroy(&list->mutex);

    // 使用临时数组存储所有节点，避免在遍历时修改next指针
    size_t capacity = atomic_load(&list->size) + 1;  // +1 for head node
    skiplist_node_t** nodes = (skiplist_node_t**)calloc(capacity, sizeof(skiplist_node_t*));
    if (!nodes) {
        ppdb_log_error("Failed to allocate nodes array for cleanup");
        return;  // 继续使用原始方法清理
    }

    // 收集所有节点
    size_t node_count = 0;
    skiplist_node_t* current = head;
    while (current && node_count < capacity) {
        nodes[node_count++] = current;
        current = current->next[0];
    }

    // 安全地销毁每个节点
    for (size_t i = 0; i < node_count; i++) {
        if (nodes[i] == head) {
            // 头节点特殊处理
            size_t head_size = sizeof(skiplist_node_t) + MAX_LEVEL * sizeof(skiplist_node_t*);
            memset(head->next, 0, MAX_LEVEL * sizeof(skiplist_node_t*));
            head->height = 0;
            head->state = NODE_DELETED;
            if (head->key) {
                memset(head->key, 0, head->key_len);
                free(head->key);
            }
            if (head->value) {
                memset(head->value, 0, head->value_len);
                free(head->value);
            }
            head->key = NULL;
            head->value = NULL;
            head->key_len = 0;
            head->value_len = 0;
            memset(head, 0, head_size);
            free(head);
        } else {
            destroy_node(nodes[i]);
        }
        nodes[i] = NULL;  // 防止重复释放
    }

    // 清理临时数组
    free(nodes);

    // 清零整个跳表结构
    memset(list, 0, sizeof(skiplist_t));

    // 最后释放跳表结构
    free(list);
}

ppdb_error_t skiplist_put(skiplist_t* list,
                         const uint8_t* key, size_t key_len,
                         const uint8_t* value, size_t value_len) {
    if (!list || !key || !value) {
        return PPDB_ERR_NULL_POINTER;
    }

    if (key_len == 0 || value_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&list->mutex);

    // Create new node
    uint32_t level = random_level();
    skiplist_node_t* new_node = create_node(level, key, key_len, value, value_len);
    if (!new_node) {
        pthread_mutex_unlock(&list->mutex);
        return PPDB_ERR_NO_MEMORY;
    }

    // Find position to insert
    skiplist_node_t* current = list->head;
    skiplist_node_t* update[MAX_LEVEL];
    for (int i = level - 1; i >= 0; i--) {
        while (current->next[i] &&
               compare_keys(current->next[i]->key, current->next[i]->key_len,
                          key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    // Check if key already exists
    current = current->next[0];
    if (current && compare_keys(current->key, current->key_len,
                              key, key_len) == 0) {
        // Update value
        uint8_t* new_value = malloc(value_len);
        if (!new_value) {
            destroy_node(new_node);
            pthread_mutex_unlock(&list->mutex);
            return PPDB_ERR_NO_MEMORY;
        }
        memcpy(new_value, value, value_len);
        free(current->value);
        current->value = new_value;
        current->value_len = value_len;
        destroy_node(new_node);
    } else {
        // Insert new node
        for (uint32_t i = 0; i < level; i++) {
            new_node->next[i] = update[i]->next[i];
            update[i]->next[i] = new_node;
        }
        atomic_fetch_add(&list->size, 1);
    }

    pthread_mutex_unlock(&list->mutex);
    return PPDB_OK;
}

ppdb_error_t skiplist_get(skiplist_t* list,
                         const uint8_t* key, size_t key_len,
                         uint8_t** value, size_t* value_len) {
    if (!list || !key || !value_len) {
        return PPDB_ERR_NULL_POINTER;
    }

    if (key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&list->mutex);

    // Find node
    skiplist_node_t* current = list->head;
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] &&
               compare_keys(current->next[i]->key, current->next[i]->key_len,
                          key, key_len) < 0) {
            current = current->next[i];
        }
    }
    current = current->next[0];

    // Check if key exists
    if (!current || compare_keys(current->key, current->key_len,
                               key, key_len) != 0) {
        pthread_mutex_unlock(&list->mutex);
        return PPDB_ERR_NOT_FOUND;
    }

    // Return value size if no buffer pointer provided
    if (!value) {
        *value_len = current->value_len;
        pthread_mutex_unlock(&list->mutex);
        return PPDB_OK;
    }

    // Allocate and copy value
    uint8_t* new_value = (uint8_t*)malloc(current->value_len);
    if (!new_value) {
        pthread_mutex_unlock(&list->mutex);
        return PPDB_ERR_NO_MEMORY;
    }
    memcpy(new_value, current->value, current->value_len);
    *value = new_value;
    *value_len = current->value_len;

    pthread_mutex_unlock(&list->mutex);
    return PPDB_OK;
}

ppdb_error_t skiplist_delete(skiplist_t* list,
                            const uint8_t* key, size_t key_len) {
    if (!list || !key) {
        return PPDB_ERR_NULL_POINTER;
    }

    if (key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&list->mutex);

    // Find node to delete
    skiplist_node_t* current = list->head;
    skiplist_node_t* update[MAX_LEVEL];
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] &&
               compare_keys(current->next[i]->key, current->next[i]->key_len,
                          key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }
    current = current->next[0];

    // Check if key exists
    if (!current || compare_keys(current->key, current->key_len,
                               key, key_len) != 0) {
        pthread_mutex_unlock(&list->mutex);
        return PPDB_ERR_NOT_FOUND;
    }

    // Update pointers
    for (uint32_t i = 0; i < current->height; i++) {
        if (update[i]->next[i] != current) {
            break;
        }
        update[i]->next[i] = current->next[i];
    }

    // Free node and update size
    destroy_node(current);
    atomic_fetch_sub(&list->size, 1);

    pthread_mutex_unlock(&list->mutex);
    return PPDB_OK;
}

size_t skiplist_size(skiplist_t* list) {
    if (!list) return 0;
    return atomic_load(&list->size);
}

skiplist_iterator_t* skiplist_iterator_create(skiplist_t* list) {
    if (!list) return NULL;

    skiplist_iterator_t* iter = malloc(sizeof(skiplist_iterator_t));
    if (!iter) {
        ppdb_log_error("Failed to allocate iterator");
        return NULL;
    }

    iter->list = list;
    iter->current = list->head;
    iter->mutex = &list->mutex;

    pthread_mutex_lock(iter->mutex);
    return iter;
}

void skiplist_iterator_destroy(skiplist_iterator_t* iter) {
    if (!iter) return;
    pthread_mutex_unlock(iter->mutex);
    memset(iter, 0, sizeof(skiplist_iterator_t));  // Zero out iterator
    free(iter);
}

bool skiplist_iterator_next(skiplist_iterator_t* iter,
                          uint8_t** key, size_t* key_size,
                          uint8_t** value, size_t* value_size) {
    if (!iter || !key || !key_size || !value || !value_size) return false;

    // Skip deleted nodes and move to next node
    do {
        iter->current = iter->current->next[0];
        if (!iter->current) return false;  // Reached end of list
    } while (iter->current->state == NODE_DELETED);

    // Return key and value
    *key = iter->current->key;
    *key_size = iter->current->key_len;
    *value = iter->current->value;
    *value_size = iter->current->value_len;

    return true;
}