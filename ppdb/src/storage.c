#include "ppdb/storage.h"
#include "ppdb/ppdb.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/sync.h"
#include <cosmopolitan.h>
#include <stdlib.h>
#include <string.h>

// 跳表操作实现
static ppdb_error_t skiplist_init(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 初始化存储级别的锁
    if (ppdb_rwlock_init(&base->storage.lock) != PPDB_OK) {
        return PPDB_ERR_LOCK_FAILED;
    }
    if (ppdb_mutex_init(&base->storage.mutex) != PPDB_OK) {
        ppdb_rwlock_destroy(&base->storage.lock);
        return PPDB_ERR_LOCK_FAILED;
    }

    base->storage.head = _calloc(1, sizeof(ppdb_node_t));
    if (!base->storage.head) {
        ppdb_rwlock_destroy(&base->storage.lock);
        ppdb_mutex_destroy(&base->storage.mutex);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化头节点的锁
    if (ppdb_rwlock_init(&base->storage.head->lock) != PPDB_OK) {
        _free(base->storage.head);
        ppdb_rwlock_destroy(&base->storage.lock);
        ppdb_mutex_destroy(&base->storage.mutex);
        return PPDB_ERR_LOCK_FAILED;
    }

    base->storage.head->height = 1;
    base->storage.head->ptr = NULL;
    base->storage.head->data = NULL;

    base->header.type = PPDB_TYPE_SKIPLIST;
    base->header.flags = 0;
    base->header.version = 1;

    base->config = *config;

    return PPDB_OK;
}

static ppdb_error_t skiplist_destroy(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = ppdb_rwlock_wrlock(&base->storage.lock);
    if (err != PPDB_OK) {
        return err;
    }

    ppdb_node_t* current = base->storage.head;
    while (current) {
        ppdb_node_t* next = (ppdb_node_t*)((uintptr_t)current->ptr & ~0ULL);
        ppdb_rwlock_destroy(&current->lock);
        _free(current->data);
        _free(current);
        current = next;
    }

    base->storage.head = NULL;

    ppdb_rwlock_unlock(&base->storage.lock);
    ppdb_rwlock_destroy(&base->storage.lock);
    ppdb_mutex_destroy(&base->storage.mutex);

    return PPDB_OK;
}

static ppdb_error_t skiplist_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = ppdb_rwlock_rdlock(&base->storage.lock);
    if (err != PPDB_OK) {
        return err;
    }

    ppdb_node_t* current = base->storage.head;
    ppdb_node_t* next;

    // 从最高层开始查找
    for (size_t level = current->height - 1; level > 0; level--) {
        err = ppdb_rwlock_rdlock(&current->lock);
        if (err != PPDB_OK) {
            ppdb_rwlock_unlock(&base->storage.lock);
            return err;
        }

        next = (ppdb_node_t*)((uintptr_t)current->ptr & ~((1ULL << level) - 1));
        while (next && memcmp(next->data, key->data, key->size) < 0) {
            ppdb_node_t* temp = current;
            current = next;
            ppdb_rwlock_unlock(&temp->lock);

            err = ppdb_rwlock_rdlock(&current->lock);
            if (err != PPDB_OK) {
                ppdb_rwlock_unlock(&base->storage.lock);
                return err;
            }

            next = (ppdb_node_t*)((uintptr_t)current->ptr & ~((1ULL << level) - 1));
        }
        ppdb_rwlock_unlock(&current->lock);
    }

    err = ppdb_rwlock_rdlock(&current->lock);
    if (err != PPDB_OK) {
        ppdb_rwlock_unlock(&base->storage.lock);
        return err;
    }

    next = (ppdb_node_t*)((uintptr_t)current->ptr & ~0ULL);
    if (next) {
        err = ppdb_rwlock_rdlock(&next->lock);
        if (err != PPDB_OK) {
            ppdb_rwlock_unlock(&current->lock);
            ppdb_rwlock_unlock(&base->storage.lock);
            return err;
        }

        if (memcmp(next->data, key->data, key->size) == 0) {
            value->data = next->data;
            value->size = next->height;
            ppdb_rwlock_unlock(&next->lock);
            ppdb_rwlock_unlock(&current->lock);
            ppdb_rwlock_unlock(&base->storage.lock);
            return PPDB_OK;
        }
        ppdb_rwlock_unlock(&next->lock);
    }

    ppdb_rwlock_unlock(&current->lock);
    ppdb_rwlock_unlock(&base->storage.lock);
    return PPDB_ERR_NOT_FOUND;
}

static ppdb_error_t skiplist_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = ppdb_rwlock_wrlock(&base->storage.lock);
    if (err != PPDB_OK) {
        return err;
    }

    ppdb_node_t* new_node = _calloc(1, sizeof(ppdb_node_t));
    if (!new_node) {
        ppdb_rwlock_unlock(&base->storage.lock);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    err = ppdb_rwlock_init(&new_node->lock);
    if (err != PPDB_OK) {
        _free(new_node);
        ppdb_rwlock_unlock(&base->storage.lock);
        return err;
    }

    new_node->data = _malloc(key->size);
    if (!new_node->data) {
        ppdb_rwlock_destroy(&new_node->lock);
        _free(new_node);
        ppdb_rwlock_unlock(&base->storage.lock);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    memcpy(new_node->data, key->data, key->size);
    new_node->height = 1;

    ppdb_node_t* current = base->storage.head;
    ppdb_node_t* next;

    err = ppdb_mutex_lock(&base->storage.mutex);
    if (err != PPDB_OK) {
        _free(new_node->data);
        ppdb_rwlock_destroy(&new_node->lock);
        _free(new_node);
        ppdb_rwlock_unlock(&base->storage.lock);
        return err;
    }

    for (size_t i = current->height - 1; i > 0; i--) {
        err = ppdb_rwlock_wrlock(&current->lock);
        if (err != PPDB_OK) {
            ppdb_mutex_unlock(&base->storage.mutex);
            _free(new_node->data);
            ppdb_rwlock_destroy(&new_node->lock);
            _free(new_node);
            ppdb_rwlock_unlock(&base->storage.lock);
            return err;
        }

        next = (ppdb_node_t*)((uintptr_t)current->ptr & ~((1ULL << i) - 1));
        while (next && memcmp(next->data, key->data, key->size) < 0) {
            ppdb_node_t* temp = current;
            current = next;
            ppdb_rwlock_unlock(&temp->lock);

            err = ppdb_rwlock_wrlock(&current->lock);
            if (err != PPDB_OK) {
                ppdb_mutex_unlock(&base->storage.mutex);
                _free(new_node->data);
                ppdb_rwlock_destroy(&new_node->lock);
                _free(new_node);
                ppdb_rwlock_unlock(&base->storage.lock);
                return err;
            }

            next = (ppdb_node_t*)((uintptr_t)next->ptr & ~((1ULL << i) - 1));
        }

        if (i < new_node->height) {
            new_node->ptr = (void*)((uintptr_t)next | ((1ULL << i) - 1));
            current->ptr = (void*)((uintptr_t)new_node | ((1ULL << i) - 1));
        }
        ppdb_rwlock_unlock(&current->lock);
    }

    err = ppdb_rwlock_wrlock(&current->lock);
    if (err != PPDB_OK) {
        ppdb_mutex_unlock(&base->storage.mutex);
        _free(new_node->data);
        ppdb_rwlock_destroy(&new_node->lock);
        _free(new_node);
        ppdb_rwlock_unlock(&base->storage.lock);
        return err;
    }

    next = (ppdb_node_t*)((uintptr_t)current->ptr & ~0ULL);
    new_node->ptr = (void*)((uintptr_t)next);
    current->ptr = (void*)((uintptr_t)new_node);

    ppdb_rwlock_unlock(&current->lock);
    ppdb_mutex_unlock(&base->storage.mutex);
    ppdb_rwlock_unlock(&base->storage.lock);

    return PPDB_OK;
}

// 内存表操作实现
static ppdb_error_t memtable_init(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_memtable_t* memtable = _calloc(1, sizeof(ppdb_memtable_t));
    if (!memtable) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    memtable->type = PPDB_MEMTABLE_TYPE_SKIPLIST;
    memtable->max_size = config->memory_limit;
    memtable->size = 0;

    ppdb_error_t err = ppdb_skiplist_create(&memtable->skiplist, PPDB_MAX_LEVEL);
    if (err != PPDB_OK) {
        _free(memtable);
        return err;
    }

    err = ppdb_sync_create(&memtable->lock);
    if (err != PPDB_OK) {
        ppdb_skiplist_destroy(memtable->skiplist);
        _free(memtable);
        return err;
    }

    base->storage.memtable = memtable;

    return PPDB_OK;
}

static ppdb_error_t memtable_destroy(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_memtable_t* memtable = base->storage.memtable;
    if (!memtable) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_skiplist_destroy(memtable->skiplist);
    ppdb_sync_destroy(memtable->lock);
    _free(memtable);

    return PPDB_OK;
}

static ppdb_error_t memtable_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_memtable_t* memtable = base->storage.memtable;
    if (!memtable) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (memtable->size >= memtable->max_size) {
        return PPDB_ERR_FULL;
    }

    ppdb_sync_lock(memtable->lock);

    ppdb_error_t err = ppdb_skiplist_put(memtable->skiplist, key->data, key->size, value->data, value->size);
    if (err == PPDB_OK) {
        memtable->size += key->size + value->size;
    }

    ppdb_sync_unlock(memtable->lock);

    return err;
}

static ppdb_error_t memtable_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_memtable_t* memtable = base->storage.memtable;
    if (!memtable) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_sync_lock(memtable->lock);

    ppdb_error_t err = ppdb_skiplist_get(memtable->skiplist, key->data, key->size, value->data, &value->size);
    ppdb_sync_unlock(memtable->lock);

    return err;
}

// 分片存储操作实现
static ppdb_error_t sharded_init(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = ppdb_rwlock_init(&base->storage.lock);
    if (err != PPDB_OK) {
        return err;
    }

    err = ppdb_mutex_init(&base->storage.mutex);
    if (err != PPDB_OK) {
        ppdb_rwlock_destroy(&base->storage.lock);
        return err;
    }

    base->array.items = _calloc(16, sizeof(void*));
    if (!base->array.items) {
        ppdb_rwlock_destroy(&base->storage.lock);
        ppdb_mutex_destroy(&base->storage.mutex);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    base->array.capacity = 16;
    base->array.size = 0;

    ppdb_storage_config_t shard_config = *config;
    shard_config.memory_limit /= 16;

    for (size_t i = 0; i < 16; i++) {
        ppdb_base_t* shard = _malloc(sizeof(ppdb_base_t));
        if (!shard) {
            for (size_t j = 0; j < i; j++) {
                skiplist_destroy(base->array.items[j]);
                _free(base->array.items[j]);
            }
            _free(base->array.items);
            ppdb_rwlock_destroy(&base->storage.lock);
            ppdb_mutex_destroy(&base->storage.mutex);
            return PPDB_ERR_OUT_OF_MEMORY;
        }

        if (skiplist_init(shard, &shard_config) != PPDB_OK) {
            for (size_t j = 0; j < i; j++) {
                skiplist_destroy(base->array.items[j]);
                _free(base->array.items[j]);
            }
            _free(base->array.items);
            ppdb_rwlock_destroy(&base->storage.lock);
            ppdb_mutex_destroy(&base->storage.mutex);
            return PPDB_ERR_INTERNAL;
        }

        base->array.items[i] = shard;
        base->array.size++;
    }

    return PPDB_OK;
}

// KV存储操作实现
static ppdb_error_t kvstore_init(ppdb_base_t* base, const ppdb_storage_config_t* config, ppdb_kvstore_t** store) {
    if (!base || !config || !store) {
        return PPDB_ERR_INVALID_ARG;
    }
    return PPDB_ERR_NOT_IMPLEMENTED;
}

// 存储同步操作实现
ppdb_error_t ppdb_storage_sync(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = ppdb_rwlock_wrlock(&base->storage.lock);
    if (err != PPDB_OK) {
        return err;
    }

    // TODO: 实现存储同步

    ppdb_rwlock_unlock(&base->storage.lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_flush(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = ppdb_rwlock_wrlock(&base->storage.lock);
    if (err != PPDB_OK) {
        return err;
    }

    // TODO: 实现存储刷新

    ppdb_rwlock_unlock(&base->storage.lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_compact(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = ppdb_rwlock_wrlock(&base->storage.lock);
    if (err != PPDB_OK) {
        return err;
    }

    // TODO: 实现存储压缩

    ppdb_rwlock_unlock(&base->storage.lock);
    return PPDB_OK;
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
    return skiplist_init(base, config);
}

ppdb_error_t ppdb_memtable_create(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }
    base->header.type = PPDB_TYPE_MEMTABLE;
    return memtable_init(base, config);
}

ppdb_error_t ppdb_sharded_create(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }
    base->header.type = PPDB_TYPE_SHARDED;
    return sharded_init(base, config);
}

ppdb_error_t ppdb_kvstore_create(ppdb_base_t* base, const ppdb_storage_config_t* config, ppdb_kvstore_t** store) {
    if (!base || !config || !store) {
        return PPDB_ERR_INVALID_ARG;
    }
    base->header.type = PPDB_TYPE_KVSTORE;
    return kvstore_init(base, config, store);
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
    // TODO: 实现KV存储销毁
}
