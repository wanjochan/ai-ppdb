#include <cosmopolitan.h>
#include "ppdb/skiplist.h"
#include "ppdb/logger.h"

#define MAX_LEVEL 32
#define P 0.25

// 跳表节点
typedef struct skipnode_t {
    uint8_t* key;
    size_t key_len;
    uint8_t* value;
    size_t value_len;
    struct skipnode_t* forward[1];  // 柔性数组
} skipnode_t;

// 跳表结构
typedef struct skiplist_t {
    int level;                     // 当前最大层数
    volatile size_t size;          // 节点数量（volatile 保证可见性）
    skipnode_t* header;            // 头节点
    pthread_mutex_t mutex;         // 互斥锁
} skiplist_t;

// 迭代器结构
typedef struct skiplist_iterator_t {
    skiplist_t* list;           // 关联的跳表
    skipnode_t* current;        // 当前节点
} skiplist_iterator_t;

// 创建节点
static skipnode_t* create_node(int level,
                             const uint8_t* key, size_t key_len,
                             const uint8_t* value, size_t value_len) {
    skipnode_t* node = malloc(sizeof(skipnode_t) + level * sizeof(skipnode_t*));
    if (!node) return NULL;

    // 为键分配内存
    node->key = malloc(key_len);
    if (!node->key) {
        free(node);
        return NULL;
    }

    // 为值分配内存
    node->value = malloc(value_len);
    if (!node->value) {
        free(node->key);
        free(node);
        return NULL;
    }

    memcpy(node->key, key, key_len);
    memcpy(node->value, value, value_len);
    node->key_len = key_len;
    node->value_len = value_len;

    // 初始化 forward 数组
    for (int i = 0; i < level; i++) {
        node->forward[i] = NULL;
    }

    return node;
}

// 销毁节点
static void destroy_node(skipnode_t* node) {
    if (!node) return;
    if (node->key) free(node->key);
    if (node->value) free(node->value);
    free(node);
}

// 随机层数（线程安全）
static int random_level(void) {
    int level = 1;
    while ((random() & 0xFFFF) < (P * 0xFFFF) && level < MAX_LEVEL) {
        level++;
    }
    return level;
}

// 创建跳表
skiplist_t* skiplist_create(void) {
    skiplist_t* list = malloc(sizeof(skiplist_t));
    if (!list) return NULL;

    list->level = 1;
    list->size = 0;

    // 初始化互斥锁
    if (pthread_mutex_init(&list->mutex, NULL) != 0) {
        free(list);
        return NULL;
    }

    // 创建头节点
    list->header = create_node(MAX_LEVEL, NULL, 0, NULL, 0);
    if (!list->header) {
        pthread_mutex_destroy(&list->mutex);
        free(list);
        return NULL;
    }

    return list;
}

// 销毁跳表
void skiplist_destroy(skiplist_t* list) {
    if (!list) return;

    // 释放所有节点
    skipnode_t* node = list->header->forward[0];
    while (node) {
        skipnode_t* next = node->forward[0];
        destroy_node(node);
        node = next;
    }

    // 释放头节点和跳表结构
    destroy_node(list->header);
    pthread_mutex_destroy(&list->mutex);
    free(list);
}

// 比较键
static int compare_key(const uint8_t* key1, size_t key1_len,
                      const uint8_t* key2, size_t key2_len) {
    // 直接比较键的内容，不考虑结尾的空字符
    size_t min_len = key1_len < key2_len ? key1_len : key2_len;
    int result = memcmp(key1, key2, min_len);
    if (result != 0) return result;
    return key1_len - key2_len;
}

// 插入/更新键值对
int skiplist_put(skiplist_t* list,
                const uint8_t* key, size_t key_len,
                const uint8_t* value, size_t value_len) {
    if (!list || !key || !value || key_len == 0 || value_len == 0) {
        return -1;
    }

    pthread_mutex_lock(&list->mutex);

    // 查找位置
    skipnode_t* update[MAX_LEVEL];
    skipnode_t* current = list->header;

    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] &&
               compare_key(current->forward[i]->key,
                         current->forward[i]->key_len,
                         key, key_len) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    // 更新已存在的键
    if (current && compare_key(current->key, current->key_len,
                             key, key_len) == 0) {
        uint8_t* new_value = malloc(value_len);
        if (!new_value) {
            pthread_mutex_unlock(&list->mutex);
            return -1;
        }

        memcpy(new_value, value, value_len);
        
        // 先保存旧值的指针，再更新新值
        uint8_t* old_value = current->value;
        current->value = new_value;
        current->value_len = value_len;
        
        // 最后释放旧值
        free(old_value);
        
        pthread_mutex_unlock(&list->mutex);
        return 0;
    }

    // 插入新节点
    int new_level = random_level();
    if (new_level > list->level) {
        for (int i = list->level; i < new_level; i++) {
            update[i] = list->header;
        }
        list->level = new_level;
    }

    skipnode_t* new_node = create_node(new_level, key, key_len,
                                     value, value_len);
    if (!new_node) {
        pthread_mutex_unlock(&list->mutex);
        return -1;
    }

    for (int i = 0; i < new_level; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    list->size++;  // 在锁的保护下更新
    pthread_mutex_unlock(&list->mutex);
    return 0;
}

// 获取键对应的值
int skiplist_get(skiplist_t* list, const uint8_t* key, size_t key_len,
                uint8_t* value, size_t* value_len) {
    if (!list || !key || !value_len) {
        return -1;
    }

    pthread_mutex_lock(&list->mutex);

    skipnode_t* current = list->header;
    skipnode_t* target = NULL;
    
    // 从最高层开始查找
    for (int32_t level = (int32_t)list->level - 1; level >= 0; level--) {
        while (current->forward[level] &&
               compare_key(current->forward[level]->key,
                         current->forward[level]->key_len,
                         key, key_len) < 0) {
            current = current->forward[level];
        }
        if (current->forward[level] &&
            compare_key(current->forward[level]->key,
                      current->forward[level]->key_len,
                      key, key_len) == 0) {
            target = current->forward[level];
        }
    }

    // 如果没有找到节点
    if (!target) {
        pthread_mutex_unlock(&list->mutex);
        return 1;  // 未找到
    }

    // 如果找到了节点,先返回值的大小
    if (!value) {
        *value_len = target->value_len;
        pthread_mutex_unlock(&list->mutex);
        return 0;  // 只是查询大小
    }
    if (*value_len < target->value_len) {
        *value_len = target->value_len;
        pthread_mutex_unlock(&list->mutex);
        return -1;  // 缓冲区太小
    }

    // 复制值
    memcpy(value, target->value, target->value_len);
    *value_len = target->value_len;
    pthread_mutex_unlock(&list->mutex);
    return 0;  // 成功
}

// 删除键值对
int skiplist_delete(skiplist_t* list,
                   const uint8_t* key, size_t key_len) {
    if (!list || !key || key_len == 0) {
        return -1;
    }

    pthread_mutex_lock(&list->mutex);

    skipnode_t* update[MAX_LEVEL];
    skipnode_t* current = list->header;

    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] &&
               compare_key(current->forward[i]->key,
                         current->forward[i]->key_len,
                         key, key_len) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];
    if (!current || compare_key(current->key, current->key_len,
                              key, key_len) != 0) {
        pthread_mutex_unlock(&list->mutex);
        return 1;  // 未找到
    }

    for (int i = 0; i < list->level; i++) {
        if (update[i]->forward[i] != current) {
            break;
        }
        update[i]->forward[i] = current->forward[i];
    }

    destroy_node(current);

    while (list->level > 1 && list->header->forward[list->level - 1] == NULL) {
        list->level--;
    }

    list->size--;  // 在锁的保护下更新
    pthread_mutex_unlock(&list->mutex);
    return 0;
}

// 获取跳表大小
size_t skiplist_size(skiplist_t* list) {
    if (!list) return 0;
    pthread_mutex_lock(&list->mutex);
    size_t size = list->size;
    pthread_mutex_unlock(&list->mutex);
    return size;
}

// 创建迭代器
skiplist_iterator_t* skiplist_iterator_create(skiplist_t* list) {
    if (!list) {
        return NULL;
    }

    skiplist_iterator_t* iter = (skiplist_iterator_t*)malloc(sizeof(skiplist_iterator_t));
    if (!iter) {
        return NULL;
    }

    iter->list = list;
    iter->current = list->header->forward[0];  // 从第一个节点开始
    return iter;
}

// 销毁迭代器
void skiplist_iterator_destroy(skiplist_iterator_t* iter) {
    if (iter) {
        free(iter);
    }
}

// 获取下一个键值对
bool skiplist_iterator_next(skiplist_iterator_t* iter,
                          uint8_t** key, size_t* key_size,
                          uint8_t** value, size_t* value_size) {
    if (!iter || !key || !key_size || !value || !value_size) {
        return false;
    }

    if (!iter->current) {
        return false;  // 已经到达末尾
    }

    *key = iter->current->key;
    *key_size = iter->current->key_len;
    *value = iter->current->value;
    *value_size = iter->current->value_len;

    iter->current = iter->current->forward[0];  // 移动到下一个节点
    return true;
}