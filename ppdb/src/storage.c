#include "ppdb/storage.h"
#include "ppdb/ppdb.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/sync.h"
#include <cosmopolitan.h>
#include <stdlib.h>
#include <string.h>

// 跳表操作实现
static ppdb_error_t skiplist_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    // 初始化存储级别的锁
    if (ppdb_rwlock_init(&base->storage.lock) != PPDB_OK) {
        return PPDB_ERR_LOCK_FAILED;
    }

    // 创建头节点
    base->storage.head = calloc(1, sizeof(ppdb_node_t));
    if (!base->storage.head) {
        ppdb_rwlock_destroy(&base->storage.lock);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化头节点的锁
    if (ppdb_rwlock_init(&base->storage.head->lock) != PPDB_OK) {
        free(base->storage.head);
        ppdb_rwlock_destroy(&base->storage.lock);
        return PPDB_ERR_LOCK_FAILED;
    }

    // 设置头节点的高度为最大层数
    base->storage.head->height = MAX_SKIPLIST_LEVEL;

    // 初始化所有层的next指针为NULL
    for (int i = 0; i < MAX_SKIPLIST_LEVEL; i++) {
        base->storage.head->next[i] = NULL;
    }

    // 初始化统计信息
    atomic_init(&base->metrics.get_count, 0);
    atomic_init(&base->metrics.get_hits, 0);
    atomic_init(&base->metrics.put_count, 0);
    atomic_init(&base->metrics.remove_count, 0);

    return PPDB_OK;
}

static ppdb_error_t skiplist_destroy(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    ppdb_node_t* current = base->storage.head;
    while (current) {
        ppdb_node_t* next = current->next[0];
        ppdb_rwlock_destroy(&current->lock);
        free(current->key);
        free(current->value);
        free(current);
        current = next;
    }

    ppdb_rwlock_destroy(&base->storage.lock);
    return PPDB_OK;
}

static ppdb_error_t skiplist_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;

    ppdb_error_t err = ppdb_rwlock_rdlock(&base->storage.lock);
    if (err != PPDB_OK) return err;

    ppdb_node_t* current = base->storage.head;
    int level = current->height - 1;

    // 从最高层开始查找
    while (level >= 0) {
        while (current->next[level] && memcmp(current->next[level]->key->data, key->data, 
               MIN(current->next[level]->key->size, key->size)) < 0) {
            current = current->next[level];
        }
        level--;
    }

    // 移动到下一个节点
    current = current->next[0];

    // 检查是否找到
    if (current && current->key->size == key->size && 
        memcmp(current->key->data, key->data, key->size) == 0) {
        value->data = current->value->data;
        value->size = current->value->size;
        atomic_fetch_add(&base->metrics.get_hits, 1);
        ppdb_rwlock_unlock(&base->storage.lock);
        return PPDB_OK;
    }

    ppdb_rwlock_unlock(&base->storage.lock);
    return PPDB_ERR_NOT_FOUND;
}

static uint32_t random_level(void) {
    uint32_t level = 1;
    while (level < MAX_SKIPLIST_LEVEL && (rand() & 1)) {
        level++;
    }
    return level;
}

static ppdb_error_t skiplist_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;

    ppdb_error_t err = ppdb_rwlock_wrlock(&base->storage.lock);
    if (err != PPDB_OK) return err;

    // 创建新节点
    ppdb_node_t* new_node = calloc(1, sizeof(ppdb_node_t));
    if (!new_node) {
        ppdb_rwlock_unlock(&base->storage.lock);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 分配并复制key
    new_node->key = malloc(sizeof(ppdb_key_t));
    if (!new_node->key) {
        free(new_node);
        ppdb_rwlock_unlock(&base->storage.lock);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    new_node->key->data = malloc(key->size);
    if (!new_node->key->data) {
        free(new_node->key);
        free(new_node);
        ppdb_rwlock_unlock(&base->storage.lock);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    memcpy(new_node->key->data, key->data, key->size);
    new_node->key->size = key->size;

    // 分配并复制value
    new_node->value = malloc(sizeof(ppdb_value_t));
    if (!new_node->value) {
        free(new_node->key->data);
        free(new_node->key);
        free(new_node);
        ppdb_rwlock_unlock(&base->storage.lock);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    new_node->value->data = malloc(value->size);
    if (!new_node->value->data) {
        free(new_node->value);
        free(new_node->key->data);
        free(new_node->key);
        free(new_node);
        ppdb_rwlock_unlock(&base->storage.lock);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    memcpy(new_node->value->data, value->data, value->size);
    new_node->value->size = value->size;

    // 初始化节点锁
    if (ppdb_rwlock_init(&new_node->lock) != PPDB_OK) {
        free(new_node->value->data);
        free(new_node->value);
        free(new_node->key->data);
        free(new_node->key);
        free(new_node);
        ppdb_rwlock_unlock(&base->storage.lock);
        return PPDB_ERR_LOCK_FAILED;
    }

    // 随机生成层数
    new_node->height = random_level();

    // 更新各层指针
    ppdb_node_t* current = base->storage.head;
    ppdb_node_t* update[MAX_SKIPLIST_LEVEL];
    int level = current->height - 1;

    // 从最高层开始查找插入位置
    while (level >= 0) {
        while (current->next[level] && memcmp(current->next[level]->key->data, key->data,
               MIN(current->next[level]->key->size, key->size)) < 0) {
            current = current->next[level];
        }
        update[level] = current;
        level--;
    }

    // 如果key已存在，更新value
    if (current->next[0] && current->next[0]->key->size == key->size &&
        memcmp(current->next[0]->key->data, key->data, key->size) == 0) {
        ppdb_node_t* old = current->next[0];
        for (int i = 0; i < old->height; i++) {
            update[i]->next[i] = new_node;
            new_node->next[i] = old->next[i];
        }
        ppdb_rwlock_destroy(&old->lock);
        free(old->value->data);
        free(old->value);
        free(old->key->data);
        free(old->key);
        free(old);
    } else {
        // 插入新节点
        for (uint32_t i = 0; i < new_node->height; i++) {
            if (i < base->storage.head->height) {
                new_node->next[i] = update[i]->next[i];
                update[i]->next[i] = new_node;
            }
        }
    }

    // 更新统计信息
    atomic_fetch_add(&base->metrics.put_count, 1);
    ppdb_rwlock_unlock(&base->storage.lock);
    return PPDB_OK;
}

static ppdb_error_t skiplist_remove(ppdb_base_t* base, const ppdb_key_t* key) {
    if (!base || !key) return PPDB_ERR_NULL_POINTER;

    ppdb_error_t err = ppdb_rwlock_wrlock(&base->storage.lock);
    if (err != PPDB_OK) return err;

    // 查找要删除的节点
    ppdb_node_t* current = base->storage.head;
    ppdb_node_t* update[MAX_SKIPLIST_LEVEL];
    int level = current->height - 1;

    // 从最高层开始查找
    while (level >= 0) {
        while (current->next[level] && memcmp(current->next[level]->key->data, key->data,
               MIN(current->next[level]->key->size, key->size)) < 0) {
            current = current->next[level];
        }
        update[level] = current;
        level--;
    }

    // 获取要删除的节点
    current = current->next[0];

    // 检查是否找到要删除的节点
    if (!current || current->key->size != key->size ||
        memcmp(current->key->data, key->data, key->size) != 0) {
        ppdb_rwlock_unlock(&base->storage.lock);
        return PPDB_ERR_NOT_FOUND;
    }

    // 更新各层指针，删除节点
    for (uint32_t i = 0; i < current->height; i++) {
        update[i]->next[i] = current->next[i];
    }

    // 清理节点资源
    ppdb_rwlock_destroy(&current->lock);
    free(current->value->data);
    free(current->value);
    free(current->key->data);
    free(current->key);
    free(current);

    // 更新统计信息
    atomic_fetch_add(&base->metrics.remove_count, 1);

    ppdb_rwlock_unlock(&base->storage.lock);
    return PPDB_OK;
}

// 内存表操作实现
static ppdb_error_t memtable_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    // 使用skiplist作为底层存储
    ppdb_error_t err = skiplist_init(base);
    if (err != PPDB_OK) return err;

    base->mem.limit = DEFAULT_MEMTABLE_SIZE;
    atomic_init(&base->mem.used, 0);

    return PPDB_OK;
}

static ppdb_error_t memtable_destroy(ppdb_base_t* base) {
    return skiplist_destroy(base);
}

static ppdb_error_t memtable_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    return skiplist_get(base, key, value);
}

static ppdb_error_t memtable_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (atomic_load(&base->mem.used) + key->size + value->size > base->mem.limit) {
        return PPDB_ERR_FULL;
    }

    ppdb_error_t err = skiplist_put(base, key, value);
    if (err == PPDB_OK) {
        atomic_fetch_add(&base->mem.used, key->size + value->size);
    }
    return err;
}

static ppdb_error_t memtable_remove(ppdb_base_t* base, const ppdb_key_t* key) {
    ppdb_value_t old_value;
    ppdb_error_t err = skiplist_get(base, key, &old_value);
    if (err != PPDB_OK) return err;

    err = skiplist_remove(base, key);
    if (err == PPDB_OK) {
        atomic_fetch_sub(&base->mem.used, key->size + old_value.size);
    }
    return err;
}

// 分片存储操作实现
static ppdb_error_t sharded_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    base->array.count = DEFAULT_SHARD_COUNT;
    base->array.ptrs = calloc(base->array.count, sizeof(void*));
    if (!base->array.ptrs) return PPDB_ERR_OUT_OF_MEMORY;

    return PPDB_OK;
}

static ppdb_error_t sharded_destroy(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    for (uint32_t i = 0; i < base->array.count; i++) {
        if (base->array.ptrs[i]) {
            ppdb_destroy(base->array.ptrs[i]);
        }
    }
    free(base->array.ptrs);
    return PPDB_OK;
}

static uint32_t hash_key(const ppdb_key_t* key) {
    // 简单的FNV-1a哈希
    uint32_t hash = 2166136261u;
    const uint8_t* data = key->data;
    for (size_t i = 0; i < key->size; i++) {
        hash ^= data[i];
        hash *= 16777619;
    }
    return hash;
}

static ppdb_error_t sharded_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    uint32_t shard = hash_key(key) % base->array.count;
    ppdb_base_t* target = base->array.ptrs[shard];
    return target ? ppdb_get(target, key, value) : PPDB_ERR_NOT_FOUND;
}

static ppdb_error_t sharded_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    uint32_t shard = hash_key(key) % base->array.count;
    ppdb_base_t* target = base->array.ptrs[shard];
    if (!target) {
        ppdb_error_t err = ppdb_create(PPDB_TYPE_SKIPLIST, &target);
        if (err != PPDB_OK) return err;
        base->array.ptrs[shard] = target;
    }
    return ppdb_put(target, key, value);
}

static ppdb_error_t sharded_remove(ppdb_base_t* base, const ppdb_key_t* key) {
    uint32_t shard = hash_key(key) % base->array.count;
    ppdb_base_t* target = base->array.ptrs[shard];
    return target ? ppdb_remove(target, key) : PPDB_ERR_NOT_FOUND;
}

// KV存储操作实现
static ppdb_error_t kvstore_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    // 创建分片存储作为底层存储
    ppdb_error_t err = sharded_init(base);
    if (err != PPDB_OK) return err;

    // 初始化统计信息
    atomic_init(&base->metrics.get_count, 0);
    atomic_init(&base->metrics.get_hits, 0);
    atomic_init(&base->metrics.put_count, 0);
    atomic_init(&base->metrics.remove_count, 0);

    return PPDB_OK;
}

static ppdb_error_t kvstore_destroy(ppdb_base_t* base) {
    return sharded_destroy(base);
}

static ppdb_error_t kvstore_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    return sharded_get(base, key, value);
}

static ppdb_error_t kvstore_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    return sharded_put(base, key, value);
}

static ppdb_error_t kvstore_remove(ppdb_base_t* base, const ppdb_key_t* key) {
    return sharded_remove(base, key);
}

// 存储同步操作实现
ppdb_error_t ppdb_storage_sync(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    switch (base->header.type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE:
            return PPDB_OK;  // 内存存储不需要同步
        case PPDB_TYPE_SHARDED:
            // 同步所有分片
            for (uint32_t i = 0; i < base->array.count; i++) {
                if (base->array.ptrs[i]) {
                    ppdb_error_t err = ppdb_storage_sync(base->array.ptrs[i]);
                    if (err != PPDB_OK) return err;
                }
            }
            return PPDB_OK;
        case PPDB_TYPE_KVSTORE:
            return ppdb_storage_sync(base->array.ptrs[0]);  // 同步主存储
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

ppdb_error_t ppdb_storage_flush(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    switch (base->header.type) {
        case PPDB_TYPE_SKIPLIST:
            return PPDB_OK;  // 基础跳表不需要刷新
        case PPDB_TYPE_MEMTABLE:
            // TODO: 实现memtable的刷新操作
            return PPDB_ERR_NOT_IMPLEMENTED;
        case PPDB_TYPE_SHARDED:
            // 刷新所有分片
            for (uint32_t i = 0; i < base->array.count; i++) {
                if (base->array.ptrs[i]) {
                    ppdb_error_t err = ppdb_storage_flush(base->array.ptrs[i]);
                    if (err != PPDB_OK) return err;
                }
            }
            return PPDB_OK;
        case PPDB_TYPE_KVSTORE:
            return ppdb_storage_flush(base->array.ptrs[0]);  // 刷新主存储
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

ppdb_error_t ppdb_storage_compact(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    switch (base->header.type) {
        case PPDB_TYPE_SKIPLIST:
            return PPDB_OK;  // 基础跳表不需要压缩
        case PPDB_TYPE_MEMTABLE:
            // TODO: 实现memtable的压缩操作
            return PPDB_ERR_NOT_IMPLEMENTED;
        case PPDB_TYPE_SHARDED:
            // 压缩所有分片
            for (uint32_t i = 0; i < base->array.count; i++) {
                if (base->array.ptrs[i]) {
                    ppdb_error_t err = ppdb_storage_compact(base->array.ptrs[i]);
                    if (err != PPDB_OK) return err;
                }
            }
            return PPDB_OK;
        case PPDB_TYPE_KVSTORE:
            return ppdb_storage_compact(base->array.ptrs[0]);  // 压缩主存储
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

// 存储统计操作实现
ppdb_error_t ppdb_storage_get_stats(ppdb_base_t* base, ppdb_storage_stats_t* stats) {
    if (!base || !stats) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = ppdb_rwlock_rdlock(&base->storage.lock);
    if (err != PPDB_OK) {
        return err;
    }

    stats->memory_usage = 0;
    stats->item_count = 0;
    stats->hit_count = 0;
    stats->miss_count = 0;

    ppdb_rwlock_unlock(&base->storage.lock);
    return PPDB_OK;
}

// 存储创建操作实现
ppdb_error_t ppdb_skiplist_create(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }
    base->header.type = PPDB_TYPE_SKIPLIST;
    return skiplist_init(base);
}

ppdb_error_t ppdb_memtable_create(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }
    base->header.type = PPDB_TYPE_MEMTABLE;
    return memtable_init(base);
}

ppdb_error_t ppdb_sharded_create(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }
    base->header.type = PPDB_TYPE_SHARDED;
    return sharded_init(base);
}

ppdb_error_t ppdb_kvstore_create(ppdb_base_t* base, const ppdb_storage_config_t* config, ppdb_kvstore_t** store) {
    if (!base || !config || !store) {
        return PPDB_ERR_INVALID_ARG;
    }
    base->header.type = PPDB_TYPE_KVSTORE;
    return kvstore_init(base);
}

// 存储销毁操作实现
void ppdb_skiplist_destroy(ppdb_base_t* base) {
    if (base) {
        skiplist_destroy(base);
    }
}

void ppdb_memtable_destroy(ppdb_base_t* base) {
    if (base) {
        memtable_destroy(base);
    }
}

void ppdb_sharded_destroy(ppdb_base_t* base) {
    if (base) {
        ppdb_rwlock_wrlock(&base->storage.lock);
        for (size_t i = 0; i < base->array.size; i++) {
            skiplist_destroy(base->array.items[i]);
            _free(base->array.items[i]);
        }
        _free(base->array.items);
        base->array.items = NULL;
        base->array.size = 0;
        base->array.capacity = 0;
        ppdb_rwlock_unlock(&base->storage.lock);
        ppdb_rwlock_destroy(&base->storage.lock);
        ppdb_mutex_destroy(&base->storage.mutex);
    }
}

void ppdb_kvstore_destroy(ppdb_kvstore_t* store) {
    if (!store) return;

    // Clean up storage backend
    ppdb_destroy(store->base);
    
    // Destroy locks
    ppdb_rwlock_destroy(&store->lock);
    
    // Free structure
    free(store);
}
