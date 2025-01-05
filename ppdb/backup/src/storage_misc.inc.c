// Skiplist Node Operations
//

typedef struct ppdb_skiplist_node {
    atomic_uint_fast32_t state;        // ACTIVE = 0, DELETED = 1
    ppdb_key_t* key;                   // 键
    ppdb_value_t* value;               // 值
    uint32_t height;                   // 高度
    struct ppdb_skiplist_node* next[]; // 后继节点数组
} PPDB_ALIGNED ppdb_skiplist_node_t;

static ppdb_skiplist_node_t* skiplist_node_create(ppdb_base_t* base, 
    const ppdb_key_t* key, const ppdb_value_t* value, uint32_t height) {
    if (!base || height == 0 || height > PPDB_MAX_LEVEL) {
        return NULL;
    }

    // 计算总大小，包括节点结构和next数组
    size_t node_size = sizeof(ppdb_skiplist_node_t) + height * sizeof(ppdb_skiplist_node_t*);
    node_size = PPDB_ALIGNED_SIZE(node_size);
    
    // 分配内存并初始化为0
    ppdb_skiplist_node_t* node = PPDB_ALIGNED_ALLOC(node_size);
    if (!node) {
        return NULL;
    }
    memset(node, 0, node_size);
    
    // 初始化状态为ACTIVE
    atomic_init(&node->state, 0);
    node->height = height;
    
    // 复制键值（如果提供）
    if (key) {
        size_t key_struct_size = PPDB_ALIGNED_SIZE(sizeof(ppdb_key_t));
        node->key = PPDB_ALIGNED_ALLOC(key_struct_size);
        if (!node->key) {
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
        memset(node->key, 0, key_struct_size);
        
        // 分配并复制key数据
        size_t key_data_size = PPDB_ALIGNED_SIZE(key->size);
        node->key->data = PPDB_ALIGNED_ALLOC(key_data_size);
        if (!node->key->data) {
            PPDB_ALIGNED_FREE(node->key);
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
        memset(node->key->data, 0, key_data_size);
        memcpy(node->key->data, key->data, key->size);
        node->key->size = key->size;
    }
    
    if (value) {
        size_t value_struct_size = PPDB_ALIGNED_SIZE(sizeof(ppdb_value_t));
        node->value = PPDB_ALIGNED_ALLOC(value_struct_size);
        if (!node->value) {
            if (node->key) {
                PPDB_ALIGNED_FREE(node->key->data);
                PPDB_ALIGNED_FREE(node->key);
            }
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
        memset(node->value, 0, value_struct_size);
        
        // 分配并复制value数据
        size_t value_data_size = PPDB_ALIGNED_SIZE(value->size);
        node->value->data = PPDB_ALIGNED_ALLOC(value_data_size);
        if (!node->value->data) {
            PPDB_ALIGNED_FREE(node->value);
            if (node->key) {
                PPDB_ALIGNED_FREE(node->key->data);
                PPDB_ALIGNED_FREE(node->key);
            }
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
        memset(node->value->data, 0, value_data_size);
        memcpy(node->value->data, value->data, value->size);
        node->value->size = value->size;
    }
    
    return node;
}

static void skiplist_node_destroy(ppdb_skiplist_node_t* node) {
    if (!node) {
        return;
    }

    // 清理key
    if (node->key) {
        if (node->key->data) {
            PPDB_ALIGNED_FREE(node->key->data);
        }
        PPDB_ALIGNED_FREE(node->key);
    }

    // 清理value
    if (node->value) {
        if (node->value->data) {
            PPDB_ALIGNED_FREE(node->value->data);
        }
        PPDB_ALIGNED_FREE(node->value);
    }

    // 清理next指针数组
    for (uint32_t i = 0; i < node->height; i++) {
        node->next[i] = NULL;
    }

    // 释放节点本身
    PPDB_ALIGNED_FREE(node);
}

static bool skiplist_node_is_deleted(ppdb_skiplist_node_t* node) {
    if (!node) {
        return true;
    }
    return atomic_load(&node->state) != 0;
}

static bool skiplist_node_try_mark_deleted(ppdb_skiplist_node_t* node) {
    if (!node) {
        return false;
    }
    
    uint32_t expected = 0;  // ACTIVE
    return atomic_compare_exchange_strong(&node->state, &expected, 1);  // DELETED
}

static ppdb_skiplist_node_t* skiplist_node_get_next(ppdb_skiplist_node_t* node, uint32_t level) {
    if (!node || level >= node->height) {
        return NULL;
    }
    return node->next[level];
}

static void skiplist_node_set_next(ppdb_skiplist_node_t* node, uint32_t level, ppdb_skiplist_node_t* next) {
    if (!node || level >= node->height) {
        return;
    }
    node->next[level] = next;
}

static bool skiplist_node_cas_next(ppdb_skiplist_node_t* node, uint32_t level, 
                                 ppdb_skiplist_node_t* expected, ppdb_skiplist_node_t* desired) {
    if (!node || level >= node->height || skiplist_node_is_deleted(node)) {
        return false;
    }

    if (node->next[level] != expected) {
        return false;
    }

    node->next[level] = desired;
    return true;
}

// Skiplist Operations
//

typedef struct ppdb_skiplist {
    ppdb_skiplist_node_t* head;     // 头节点
    ppdb_sync_t* level_locks;       // 层级锁数组
    uint32_t max_level;             // 最大层数
    atomic_size_t size;             // 节点数量
} PPDB_ALIGNED ppdb_skiplist_t;

static ppdb_skiplist_t* skiplist_create(ppdb_base_t* base, uint32_t max_level) {
    if (!base || max_level == 0 || max_level > PPDB_MAX_LEVEL) {
        return NULL;
    }

    // 分配跳表结构
    ppdb_skiplist_t* list = PPDB_ALIGNED_ALLOC(sizeof(ppdb_skiplist_t));
    if (!list) {
        return NULL;
    }
    memset(list, 0, sizeof(ppdb_skiplist_t));
    
    // 初始化头节点（空节点）
    list->head = skiplist_node_create(base, NULL, NULL, max_level);
    if (!list->head) {
        PPDB_ALIGNED_FREE(list);
        return NULL;
    }
    
    // 初始化层级锁
    size_t locks_size = max_level * sizeof(ppdb_sync_t);
    list->level_locks = PPDB_ALIGNED_ALLOC(locks_size);
    if (!list->level_locks) {
        skiplist_node_destroy(list->head);
        PPDB_ALIGNED_FREE(list);
        return NULL;
    }
    memset(list->level_locks, 0, locks_size);
    
    // 初始化每一层的锁
    ppdb_sync_config_t lock_config = {
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = base->config.use_lockfree,
        .enable_ref_count = false
    };
    
    for (uint32_t i = 0; i < max_level; i++) {
        ppdb_error_t err = ppdb_sync_create(&list->level_locks[i], &lock_config);
        if (err != PPDB_OK) {
            // 清理已创建的锁
            for (uint32_t j = 0; j < i; j++) {
                ppdb_sync_destroy(&list->level_locks[j]);
            }
            PPDB_ALIGNED_FREE(list->level_locks);
            skiplist_node_destroy(list->head);
            PPDB_ALIGNED_FREE(list);
            return NULL;
        }
    }
    
    list->max_level = max_level;
    atomic_init(&list->size, 0);
    
    return list;
}

static void skiplist_destroy(ppdb_skiplist_t* list) {
    if (!list) {
        return;
    }
    
    // 清理所有节点
    ppdb_skiplist_node_t* curr = list->head;
    while (curr) {
        ppdb_skiplist_node_t* next = curr->next[0];
        skiplist_node_destroy(curr);
        curr = next;
    }
    
    // 清理层级锁
    for (uint32_t i = 0; i < list->max_level; i++) {
        ppdb_sync_destroy(&list->level_locks[i]);
    }
    PPDB_ALIGNED_FREE(list->level_locks);
    
    // 释放跳表结构
    PPDB_ALIGNED_FREE(list);
}

// 生成随机层数
static uint32_t skiplist_random_level(ppdb_skiplist_t* list) {
    uint32_t level = 1;
    while (level < list->max_level && (rand() & 0xFFFF) < 0x5555) {
        level++;
    }
    return level;
}

// Skiplist Core Operations
//

static ppdb_error_t skiplist_find(ppdb_skiplist_t* list, const ppdb_key_t* key, 
                                ppdb_skiplist_node_t** node, ppdb_skiplist_node_t** update) {
    if (!list || !key) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    ppdb_skiplist_node_t* current = list->head;
    
    // 从最高层开始查找
    for (int i = list->max_level - 1; i >= 0; i--) {
        // 获取当前层的读锁
        ppdb_error_t err = ppdb_sync_read_lock(&list->level_locks[i]);
        if (err != PPDB_OK) {
            return err;
        }
        
        while (current->next[i] && !skiplist_node_is_deleted(current->next[i]) &&
               memcmp(current->next[i]->key->data, key->data, 
                     MIN(current->next[i]->key->size, key->size)) < 0) {
            current = current->next[i];
        }
        
        if (update) {
            update[i] = current;
        }
        
        ppdb_sync_read_unlock(&list->level_locks[i]);
    }
    
    if (node) {
        *node = current->next[0];
    }
    
    return PPDB_OK;
}

static ppdb_error_t skiplist_insert(ppdb_skiplist_t* list, const ppdb_key_t* key, 
                                  const ppdb_value_t* value) {
    if (!list || !key || !value) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }
    
    ppdb_skiplist_node_t* update[PPDB_MAX_LEVEL];
    ppdb_skiplist_node_t* node = NULL;
    
    // 查找插入位置
    ppdb_error_t err = skiplist_find(list, key, &node, update);
    if (err != PPDB_OK) {
        return err;
    }
    
    // 检查是否已存在
    if (node && !skiplist_node_is_deleted(node) &&
        node->key->size == key->size &&
        memcmp(node->key->data, key->data, key->size) == 0) {
        return PPDB_ERR_ALREADY_EXISTS;
    }
    
    // 生成随机层数
    uint32_t level = skiplist_random_level(list);
    
    // 创建新节点
    ppdb_skiplist_node_t* new_node = skiplist_node_create(list->base, key, value, level);
    if (!new_node) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    
    // 从底层到顶层插入
    for (uint32_t i = 0; i < level; i++) {
        // 获取写锁
        err = ppdb_sync_write_lock(&list->level_locks[i]);
        if (err != PPDB_OK) {
            // 清理已获取的锁
            for (uint32_t j = 0; j < i; j++) {
                ppdb_sync_write_unlock(&list->level_locks[j]);
            }
            skiplist_node_destroy(new_node);
            return err;
        }
        
        // 更新指针
        new_node->next[i] = update[i]->next[i];
        update[i]->next[i] = new_node;
        
        // 释放写锁
        ppdb_sync_write_unlock(&list->level_locks[i]);
    }
    
    // 更新节点计数
    atomic_fetch_add(&list->size, 1);
    
    return PPDB_OK;
}

static ppdb_error_t skiplist_remove(ppdb_skiplist_t* list, const ppdb_key_t* key) {
    if (!list || !key) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }
    
    ppdb_skiplist_node_t* update[PPDB_MAX_LEVEL];
    ppdb_skiplist_node_t* node = NULL;
    
    // 查找要删除的节点
    ppdb_error_t err = skiplist_find(list, key, &node, update);
    if (err != PPDB_OK) {
        return err;
    }
    
    // 检查节点是否存在且匹配
    if (!node || skiplist_node_is_deleted(node) ||
        node->key->size != key->size ||
        memcmp(node->key->data, key->data, key->size) != 0) {
        return PPDB_ERR_NOT_FOUND;
    }
    
    // 标记节点为已删除
    if (!skiplist_node_try_mark_deleted(node)) {
        return PPDB_ERR_BUSY;
    }
    
    // 从每一层中移除节点
    for (uint32_t i = 0; i < node->height; i++) {
        // 获取写锁
        err = ppdb_sync_write_lock(&list->level_locks[i]);
        if (err != PPDB_OK) {
            continue;  // 即使失败也继续尝试其他层
        }
        
        // 如果update[i]的next仍然指向node，则更新指针
        if (update[i]->next[i] == node) {
            update[i]->next[i] = node->next[i];
        }
        
        // 释放写锁
        ppdb_sync_write_unlock(&list->level_locks[i]);
    }
    
    // 更新节点计数
    atomic_fetch_sub(&list->size, 1);
    
    return PPDB_OK;
}

static ppdb_error_t skiplist_get(ppdb_skiplist_t* list, const ppdb_key_t* key, 
                                ppdb_value_t* value) {
    if (!list || !key || !value) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }
    
    ppdb_skiplist_node_t* node = NULL;
    
    // 查找节点
    ppdb_error_t err = skiplist_find(list, key, &node, NULL);
    if (err != PPDB_OK) {
        return err;
    }
    
    // 检查节点是否存在且匹配
    if (!node || skiplist_node_is_deleted(node) ||
        node->key->size != key->size ||
        memcmp(node->key->data, key->data, key->size) != 0) {
        return PPDB_ERR_NOT_FOUND;
    }
    
    // 复制值
    value->size = node->value->size;
    value->data = PPDB_ALIGNED_ALLOC(value->size);
    if (!value->data) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    
    memcpy(value->data, node->value->data, value->size);
    
    return PPDB_OK;
}

// Storage Operations
//
