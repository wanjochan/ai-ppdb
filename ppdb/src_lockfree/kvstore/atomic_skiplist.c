#include <cosmopolitan.h>
#include "atomic_skiplist.h"
#include "ppdb/logger.h"

// 随机层数生成
static uint32_t random_level(uint32_t max_level) {
    uint32_t level = 1;
    while ((rand() & 0xFFFF) < (0xFFFF >> 1) && level < max_level) {
        level++;
    }
    return level;
}

// 创建跳表节点
static skiplist_node_t* create_node(const char* key, uint32_t key_len,
                                  const void* value, uint32_t value_len,
                                  uint32_t level) {
    size_t node_size = sizeof(skiplist_node_t) + level * sizeof(_Atomic(skiplist_node_t*));
    skiplist_node_t* node = (skiplist_node_t*)malloc(node_size);
    if (!node) {
        ppdb_log_error("Failed to allocate skiplist node");
        return NULL;
    }

    // 分配并复制键
    node->key = (char*)malloc(key_len);
    if (!node->key) {
        free(node);
        ppdb_log_error("Failed to allocate key buffer");
        return NULL;
    }
    memcpy(node->key, key, key_len);
    node->key_len = key_len;

    // 分配并复制值
    node->value = malloc(value_len);
    if (!node->value) {
        free(node->key);
        free(node);
        ppdb_log_error("Failed to allocate value buffer");
        return NULL;
    }
    memcpy(node->value, value, value_len);
    node->value_len = value_len;

    // 初始化其他字段
    node->level = level;
    atomic_init(&node->state, NODE_VALID);
    for (uint32_t i = 0; i < level; i++) {
        atomic_init(&node->next[i], NULL);
    }

    // 创建引用计数
    node->ref_count = ref_count_create(node, NULL);
    if (!node->ref_count) {
        free(node->value);
        free(node->key);
        free(node);
        ppdb_log_error("Failed to create reference count");
        return NULL;
    }

    return node;
}

// 创建跳表
atomic_skiplist_t* atomic_skiplist_create(uint32_t max_level) {
    if (max_level > MAX_LEVEL) {
        max_level = MAX_LEVEL;
    }

    atomic_skiplist_t* list = (atomic_skiplist_t*)malloc(sizeof(atomic_skiplist_t));
    if (!list) {
        ppdb_log_error("Failed to allocate skiplist");
        return NULL;
    }

    // 创建头节点
    list->head = create_node("", 0, "", 0, max_level);
    if (!list->head) {
        free(list);
        ppdb_log_error("Failed to create head node");
        return NULL;
    }

    atomic_init(&list->size, 0);
    list->max_level = max_level;
    return list;
}

// 销毁跳表
void atomic_skiplist_destroy(atomic_skiplist_t* list) {
    if (!list) return;

    skiplist_node_t* current = list->head;
    while (current) {
        skiplist_node_t* next = atomic_load(&current->next[0]);
        ref_count_dec(current->ref_count);
        current = next;
    }
    free(list);
}

// 比较键
static int compare_keys(const char* key1, uint32_t key1_len,
                       const char* key2, uint32_t key2_len) {
    uint32_t min_len = key1_len < key2_len ? key1_len : key2_len;
    int result = memcmp(key1, key2, min_len);
    if (result != 0) return result;
    return (int)key1_len - (int)key2_len;
}

// 查找键值对
bool atomic_skiplist_find(atomic_skiplist_t* list, const char* key, uint32_t key_len,
                         void** value, uint32_t* value_len) {
    skiplist_node_t* current = list->head;
    
    // 从最高层开始查找
    for (int32_t level = (int32_t)list->max_level - 1; level >= 0; level--) {
        skiplist_node_t* next;
        do {
            next = atomic_load_explicit(&current->next[level], memory_order_acquire);
            if (!next) break;

            int cmp = compare_keys(key, key_len, next->key, next->key_len);
            if (cmp < 0) break;
            if (cmp == 0 && atomic_load(&next->state) == NODE_VALID) {
                *value = next->value;
                *value_len = next->value_len;
                return true;
            }
            current = next;
        } while (1);
    }
    return false;
}

// 插入键值对
bool atomic_skiplist_insert(atomic_skiplist_t* list, const char* key, uint32_t key_len,
                          const void* value, uint32_t value_len) {
    skiplist_node_t* update[MAX_LEVEL];
    skiplist_node_t* current = list->head;
    skiplist_node_t* found = NULL;

    // 找到每一层的插入位置，同时检查是否已存在相同的键
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
                    // 键已存在且有效
                    return false;
                }
                found = next;
                update[i] = current;
                break;
            }
            current = next;
        } while (1);
    }

    // 如果找到已删除的节点，尝试重用它
    if (found) {
        uint32_t expected = NODE_DELETED;
        if (atomic_compare_exchange_strong_explicit(
            &found->state,
            &expected,
            NODE_VALID,
            memory_order_release,
            memory_order_relaxed)) {
            // 更新值
            void* new_value = malloc(value_len);
            if (!new_value) {
                atomic_store(&found->state, NODE_DELETED);
                return false;
            }
            memcpy(new_value, value, value_len);
            void* old_value = found->value;
            found->value = new_value;
            found->value_len = value_len;
            free(old_value);
            atomic_fetch_add_explicit(&list->size, 1, memory_order_relaxed);
            return true;
        }
    }

    // 创建新节点
    uint32_t level = random_level(list->max_level);
    skiplist_node_t* new_node = create_node(key, key_len, value, value_len, level);
    if (!new_node) return false;

    // 插入新节点
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
    return true;
}

// 删除键值对
bool atomic_skiplist_delete(atomic_skiplist_t* list, const char* key, uint32_t key_len) {
    skiplist_node_t* update[MAX_LEVEL];
    skiplist_node_t* current = list->head;
    skiplist_node_t* target = NULL;

    // 找到每一层的前驱节点
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

    if (!target || atomic_load(&target->state) != NODE_VALID) {
        return false;
    }

    // 标记节点为已删除
    uint32_t expected = NODE_VALID;
    if (!atomic_compare_exchange_strong_explicit(
        &target->state,
        &expected,
        NODE_DELETED,
        memory_order_release,
        memory_order_relaxed)) {
        return false;
    }

    // 从每一层中删除节点
    for (uint32_t i = 0; i < target->level; i++) {
        do {
            skiplist_node_t* next = atomic_load_explicit(&target->next[i], memory_order_acquire);
            if (atomic_compare_exchange_weak_explicit(
                &update[i]->next[i],
                &target,
                next,
                memory_order_release,
                memory_order_relaxed)) {
                break;
            }
        } while (1);
    }

    atomic_fetch_sub_explicit(&list->size, 1, memory_order_relaxed);
    ref_count_dec(target->ref_count);
    return true;
}

// 获取元素个数
uint32_t atomic_skiplist_size(atomic_skiplist_t* list) {
    return atomic_load_explicit(&list->size, memory_order_relaxed);
}

// 清空跳表
void atomic_skiplist_clear(atomic_skiplist_t* list) {
    skiplist_node_t* current = atomic_load(&list->head->next[0]);
    while (current) {
        skiplist_node_t* next = atomic_load(&current->next[0]);
        ref_count_dec(current->ref_count);
        current = next;
    }

    // 重置头节点的所有next指针
    for (uint32_t i = 0; i < list->max_level; i++) {
        atomic_store(&list->head->next[i], NULL);
    }
    atomic_store(&list->size, 0);
}

// 遍历跳表
void atomic_skiplist_foreach(atomic_skiplist_t* list, skiplist_visitor_t visitor, void* ctx) {
    skiplist_node_t* current = atomic_load(&list->head->next[0]);
    while (current) {
        if (atomic_load(&current->state) == NODE_VALID) {
            if (!visitor(current->key, current->key_len,
                        current->value, current->value_len, ctx)) {
                break;
            }
        }
        current = atomic_load(&current->next[0]);
    }
}
