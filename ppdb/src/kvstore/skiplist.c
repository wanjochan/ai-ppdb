#include <cosmopolitan.h>
#include "skiplist.h"
#include "../common/logger.h"

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
struct skiplist_t {
    int level;                     // 当前最大层数
    size_t size;                   // 节点数量
    skipnode_t* header;            // 头节点
};

// 创建节点
static skipnode_t* create_node(int level,
                             const uint8_t* key, size_t key_len,
                             const uint8_t* value, size_t value_len) {
    skipnode_t* node = malloc(sizeof(skipnode_t) + level * sizeof(skipnode_t*));
    if (!node) return NULL;

    // 为键分配内存并确保结尾有空字符
    node->key = malloc(key_len + 1);
    if (!node->key) {
        free(node);
        return NULL;
    }

    // 为值分配内存并确保结尾有空字符
    node->value = malloc(value_len + 1);
    if (!node->value) {
        free(node->key);
        free(node);
        return NULL;
    }

    memcpy(node->key, key, key_len);
    memcpy(node->value, value, value_len);
    // 确保字符串以空字符结尾
    ((uint8_t*)node->key)[key_len] = '\0';
    ((uint8_t*)node->value)[value_len] = '\0';
    node->key_len = key_len;
    node->value_len = value_len;

    return node;
}

// 销毁节点
static void destroy_node(skipnode_t* node) {
    if (!node) return;
    free(node->key);
    free(node->value);
    free(node);
}

// 随机层数
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

    // 创建头节点
    list->header = create_node(MAX_LEVEL, NULL, 0, NULL, 0);
    if (!list->header) {
        free(list);
        return NULL;
    }

    // 初始化头节点的forward指针
    for (int i = 0; i < MAX_LEVEL; i++) {
        list->header->forward[i] = NULL;
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
    free(list);
}

// 比较键
static int compare_key(const uint8_t* key1, size_t key1_len,
                      const uint8_t* key2, size_t key2_len) {
    // 不包含结尾空字符的长度
    size_t real_len1 = key1_len;
    size_t real_len2 = key2_len;
    if (key1[key1_len - 1] == '\0') real_len1--;
    if (key2[key2_len - 1] == '\0') real_len2--;

    size_t min_len = real_len1 < real_len2 ? real_len1 : real_len2;
    int result = memcmp(key1, key2, min_len);
    if (result != 0) return result;
    return real_len1 - real_len2;
}

// 插入/更新键值对
int skiplist_put(skiplist_t* list,
                const uint8_t* key, size_t key_len,
                const uint8_t* value, size_t value_len) {
    if (!list || !key || !value || key_len == 0 || value_len == 0) {
        return -1;
    }

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
        uint8_t* new_value = malloc(value_len + 1);
        if (!new_value) return -1;

        memcpy(new_value, value, value_len);
        ((uint8_t*)new_value)[value_len] = '\0';
        free(current->value);
        current->value = new_value;
        current->value_len = value_len;
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
    if (!new_node) return -1;

    for (int i = 0; i < new_level; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    list->size++;
    return 0;
}

// 获取键对应的值
int skiplist_get(skiplist_t* list,
                const uint8_t* key, size_t key_len,
                uint8_t* value, size_t* value_len) {
    if (!list || !key || !value || !value_len || key_len == 0) {
        return -1;
    }

    skipnode_t* current = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] &&
               compare_key(current->forward[i]->key,
                         current->forward[i]->key_len,
                         key, key_len) < 0) {
            current = current->forward[i];
        }
    }

    current = current->forward[0];
    if (!current || compare_key(current->key, current->key_len,
                              key, key_len) != 0) {
        return 1;  // 未找到
    }

    if (*value_len < current->value_len) {
        *value_len = current->value_len;
        return -1;  // 缓冲区太小
    }

    memcpy(value, current->value, current->value_len);
    ((uint8_t*)value)[current->value_len] = '\0';
    *value_len = current->value_len;
    return 0;
}

// 删除键值对
int skiplist_delete(skiplist_t* list,
                   const uint8_t* key, size_t key_len) {
    if (!list || !key || key_len == 0) {
        return -1;
    }

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

    list->size--;
    return 0;
}

// 获取跳表大小
size_t skiplist_size(skiplist_t* list) {
    return list ? list->size : 0;
}