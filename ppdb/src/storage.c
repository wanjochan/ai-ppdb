#include "ppdb/ppdb.h"
#include <cosmopolitan.h>

// 前向声明
static uint64_t lemur64(void);
static ppdb_error_t skiplist_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
static ppdb_error_t skiplist_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
static ppdb_error_t skiplist_init(ppdb_base_t* base);
static ppdb_error_t skiplist_destroy(ppdb_base_t* base);
static ppdb_error_t memtable_init(ppdb_base_t* base);
static ppdb_error_t memtable_destroy(ppdb_base_t* base);
static ppdb_error_t memtable_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
static ppdb_error_t memtable_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
static ppdb_error_t memtable_flush(ppdb_base_t* base);
static ppdb_error_t sharded_init(ppdb_base_t* base);
static ppdb_error_t sharded_destroy(ppdb_base_t* base);
static ppdb_error_t sharded_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
static ppdb_error_t sharded_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
static ppdb_error_t kvstore_init(ppdb_base_t* base);
static ppdb_error_t kvstore_destroy(ppdb_base_t* base);
static ppdb_error_t kvstore_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
static ppdb_error_t kvstore_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
static ppdb_error_t kvstore_sync(ppdb_base_t* base);
static ppdb_error_t kvstore_compact(ppdb_base_t* base);
static ppdb_error_t kvstore_backup(ppdb_base_t* base, const char* path);
static ppdb_error_t kvstore_restore(ppdb_base_t* base, const char* path);
static ppdb_error_t kvstore_iterator(ppdb_base_t* base, void** iter);
static ppdb_error_t kvstore_next(void* iter, ppdb_key_t* key, ppdb_value_t* value);
static void kvstore_iterator_destroy(void* iter);
static ppdb_node_t* node_create(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value, uint32_t height);
static void node_destroy(ppdb_node_t* node);
static void node_ref(ppdb_node_t* node);
static void node_unref(ppdb_node_t* node);
static uint32_t random_level(void);

// MurmurHash3实现
static void MurmurHash3_x86_32(const void* key, int len, uint32_t seed, void* out) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 4;
    uint32_t h1 = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);
    
    for (int i = -nblocks; i; i--) {
        uint32_t k1 = blocks[i];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }
    
    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;
    switch (len & 3) {
        case 3:
            k1 ^= tail[2] << 16;
            /* fallthrough */
        case 2:
            k1 ^= tail[1] << 8;
            /* fallthrough */
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = (k1 << 15) | (k1 >> 17);
            k1 *= c2;
            h1 ^= k1;
    }
    
    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    
    *(uint32_t*)out = h1;
}

// lemur64快速随机数生成器实现
static pthread_key_t lemur_key;
static pthread_once_t lemur_key_once = PTHREAD_ONCE_INIT;

static void init_lemur_key(void) {
    pthread_key_create(&lemur_key, free);
}

static uint64_t lemur64(void) {
    pthread_once(&lemur_key_once, init_lemur_key);
    
    uint64_t* x = pthread_getspecific(lemur_key);
    if (x == NULL) {
        x = malloc(sizeof(uint64_t));
        *x = (uint64_t)time(NULL) ^ (uint64_t)pthread_self();
        pthread_setspecific(lemur_key, x);
    }
    
    *x += 0x9e3779b97f4a7c15;
    uint64_t z = *x;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

// skiplist_put实现
static ppdb_error_t skiplist_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;

    ppdb_error_t err;
    // 生成随机层数
    uint32_t height = random_level();
    ppdb_node_t* new_node = node_create(base, key, value, height);
    if (!new_node) return PPDB_ERR_OUT_OF_MEMORY;

    // 获取全局写锁
    if ((err = ppdb_sync_write_lock(base->storage.lock)) != PPDB_OK) {
        node_destroy(new_node);
        return err;
    }

    ppdb_node_t* update[MAX_SKIPLIST_LEVEL];
    ppdb_node_t* current = base->storage.head;
    
    // 从最高层开始查找插入位置
    for (int level = MAX_SKIPLIST_LEVEL - 1; level >= 0; level--) {
        while (current->next[level]) {
            ppdb_node_t* next = current->next[level];
            
            // 跳过已删除的节点
            if (ppdb_sync_counter_load(&next->is_deleted) || 
                ppdb_sync_counter_load(&next->is_garbage)) {
                current = next;
                continue;
            }
            
            int cmp = memcmp(next->key->data, key->data,
                           MIN(next->key->size, key->size));
            if (cmp > 0 || (cmp == 0 && next->key->size > key->size)) break;
            
            current = next;
        }
        update[level] = current;
    }
    
    // 检查是否已存在相同的key
    ppdb_node_t* next = current->next[0];
    if (next && next->key->size == key->size &&
        memcmp(next->key->data, key->data, key->size) == 0) {
        ppdb_sync_write_unlock(base->storage.lock);
        node_destroy(new_node);
        return PPDB_ERR_ALREADY_EXISTS;
    }
    
    // 更新指针
    for (uint32_t i = 0; i < height; i++) {
        new_node->next[i] = update[i]->next[i];
        update[i]->next[i] = new_node;
    }
    
    // 释放全局锁
    ppdb_sync_write_unlock(base->storage.lock);
    
    ppdb_sync_counter_add(&base->metrics.put_count, 1);
    return PPDB_OK;
}

// 获取节点高度
static uint32_t node_get_height(ppdb_node_t* node) {
    return ppdb_sync_counter_load(&node->height);
}

// 创建新节点
static ppdb_node_t* node_create(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value, uint32_t height) {
    if (!base || !key || !value) return NULL;
    
    size_t node_size = sizeof(ppdb_node_t) + height * sizeof(ppdb_node_t*);
    ppdb_node_t* node = PPDB_ALIGNED_ALLOC(node_size);
    if (!node) return NULL;

    // 初始化计数器
    ppdb_sync_counter_init(&node->height, height);
    ppdb_sync_counter_init(&node->is_deleted, 0);  // false
    ppdb_sync_counter_init(&node->is_garbage, 0);  // false
    ppdb_sync_counter_init(&node->ref_count, 1);
    
    // 清零next数组
    memset(node->next, 0, height * sizeof(ppdb_node_t*));

    // 使用对齐分配
    node->key = PPDB_ALIGNED_ALLOC(sizeof(ppdb_key_t));
    if (!node->key) goto fail_key;
    node->key->data = PPDB_ALIGNED_ALLOC(key->size);
    if (!node->key->data) goto fail_key_data;
    node->key->size = key->size;
    ppdb_sync_counter_init(&node->key->ref_count, 1);
    memcpy(node->key->data, key->data, key->size);

    node->value = PPDB_ALIGNED_ALLOC(sizeof(ppdb_value_t));
    if (!node->value) goto fail_value;
    node->value->data = PPDB_ALIGNED_ALLOC(value->size);
    if (!node->value->data) goto fail_value_data;
    node->value->size = value->size;
    ppdb_sync_counter_init(&node->value->ref_count, 1);
    memcpy(node->value->data, value->data, value->size);

    // 初始化节点锁
    if (ppdb_sync_create(&node->lock, &(ppdb_sync_config_t){
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = base->config.use_lockfree,  // 使用base配置中的锁模式
        .max_readers = 32,
        .backoff_us = 1,
        .max_retries = 100
    }) != PPDB_OK) goto fail_lock;

    return node;

fail_lock:
    PPDB_ALIGNED_FREE(node->value->data);
fail_value_data:
    PPDB_ALIGNED_FREE(node->value);
fail_value:
    PPDB_ALIGNED_FREE(node->key->data);
fail_key_data:
    PPDB_ALIGNED_FREE(node->key);
fail_key:
    PPDB_ALIGNED_FREE(node);
    return NULL;
}

// Fixed node destruction with proper lock handling
static void node_destroy(ppdb_node_t* node) {
    if (!node) return;
    
    // 如果已经获得了写锁，直接销毁
    if (ppdb_sync_try_write_lock(node->lock) == PPDB_OK) {
        ppdb_sync_destroy(node->lock);
        if (node->value) {
            PPDB_ALIGNED_FREE(node->value->data);
            PPDB_ALIGNED_FREE(node->value);
        }
        if (node->key) {
            PPDB_ALIGNED_FREE(node->key->data);
            PPDB_ALIGNED_FREE(node->key);
        }
        PPDB_ALIGNED_FREE(node);
    }
    // 否则标记为已删除，等待GC清理
    else {
        ppdb_sync_counter_store(&node->is_deleted, 1);  // true
        ppdb_sync_counter_store(&node->is_garbage, 1);  // true
    }
}

static void node_ref(ppdb_node_t* node) {
    ppdb_sync_counter_add(&node->ref_count, 1);
}

static void node_unref(ppdb_node_t* node) {
    if (ppdb_sync_counter_sub(&node->ref_count, 1) == 1) {
        node_destroy(node);
    }
}

// 生成随机层数（使用快速随机数生成器）
static uint32_t random_level(void) {
    uint32_t level = 1;
    uint32_t rnd = lemur64() & 0xFFFFFFFF;  // 使用lemur64快速随机数
    while ((rnd & 1) && level < MAX_SKIPLIST_LEVEL) {
        level++;
        rnd >>= 1;
    }
    return level;
}

// 跳表操作实现
static ppdb_error_t skiplist_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    // 创建头节点（使用最大层数）
    ppdb_key_t dummy_key = {NULL, 0};
    ppdb_value_t dummy_value = {NULL, 0};
    base->storage.head = node_create(base, &dummy_key, &dummy_value, MAX_SKIPLIST_LEVEL);
    if (!base->storage.head) return PPDB_ERR_OUT_OF_MEMORY;

    // 初始化存储级别的锁
    if (ppdb_sync_create(&base->storage.lock, &(ppdb_sync_config_t){
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = base->config.use_lockfree,  // 使用base配置中的锁模式
        .max_readers = 1024,
        .backoff_us = 1,
        .max_retries = 100
    }) != PPDB_OK) {
        node_destroy(base->storage.head);
        return PPDB_ERR_LOCK_FAILED;
    }

    // 初始化统计信息
    ppdb_sync_counter_init(&base->metrics.get_count, 0);
    ppdb_sync_counter_init(&base->metrics.get_hits, 0);
    ppdb_sync_counter_init(&base->metrics.put_count, 0);
    ppdb_sync_counter_init(&base->metrics.remove_count, 0);

    return PPDB_OK;
}

static ppdb_error_t skiplist_destroy(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    // 获取写锁防止并发访问
    ppdb_error_t err = ppdb_sync_write_lock(base->storage.lock);
    if (err != PPDB_OK) return err;

    ppdb_node_t* current = base->storage.head;
    while (current) {
        ppdb_node_t* next = current->next[0];
        node_unref(current);  // 使用引用计数机制替代直接释放
        current = next;
    }

    ppdb_sync_write_unlock(base->storage.lock);
    ppdb_sync_destroy(base->storage.lock);
    return PPDB_OK;
}

static ppdb_error_t memtable_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    ppdb_error_t err = skiplist_init(base);
    if (err != PPDB_OK) return err;

    base->mem.limit = DEFAULT_MEMTABLE_SIZE;
    ppdb_sync_counter_init(&base->mem.used, sizeof(ppdb_node_t));
    
    // 初始化刷盘锁
    if (ppdb_sync_create(&base->mem.flush_lock, &(ppdb_sync_config_t){
        .type = PPDB_SYNC_MUTEX,
        .use_lockfree = false,
        .backoff_us = 1,
        .max_retries = 100
    }) != PPDB_OK) {
        skiplist_destroy(base);
        return PPDB_ERR_LOCK_FAILED;
    }

    return PPDB_OK;
}

static ppdb_error_t memtable_destroy(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    ppdb_sync_destroy(base->mem.flush_lock);
    return skiplist_destroy(base);
}

static ppdb_error_t memtable_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    return skiplist_get(base, key, value);
}

static ppdb_error_t memtable_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;

    // 预先计算随机层数，确保内存使用统计准确
    uint32_t height = random_level();
    size_t node_size = sizeof(ppdb_node_t) + height * sizeof(ppdb_node_t*);
    size_t total_size = node_size + key->size + value->size;

    // 使用CAS操作检查和更新内存使用量
    while (1) {
        size_t current = ppdb_sync_counter_load(&base->mem.used);
        if (current + total_size > base->mem.limit) {
            // 获取刷盘锁
            if (ppdb_sync_lock(base->mem.flush_lock) != PPDB_OK) {
                return PPDB_ERR_BUSY;
            }

            // 在锁内再次检查
            current = ppdb_sync_counter_load(&base->mem.used);
            if (current + total_size > base->mem.limit) {
                ppdb_error_t err = memtable_flush(base);
                if (err != PPDB_OK) {
                    ppdb_sync_unlock(base->mem.flush_lock);
                    return err;
                }
            }
            ppdb_sync_unlock(base->mem.flush_lock);
            continue;  // 重试内存检查
        }

        if (ppdb_sync_counter_cas(&base->mem.used, current, current + total_size)) {
            break;  // 成功更新
        }
    }

    // 执行插入操作
    ppdb_error_t err = skiplist_put(base, key, value);
    if (err != PPDB_OK) {
        // 如果插入失败，恢复内存使用量
        ppdb_sync_counter_sub(&base->mem.used, total_size);
    }
    return err;
}

static ppdb_error_t skiplist_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;

    ppdb_sync_counter_add(&base->metrics.get_count, 1);
    ppdb_node_t* current = base->storage.head;
    
    // 获取存储级别的读锁
    if (ppdb_sync_read_lock(base->storage.lock) != PPDB_OK) {
        return PPDB_ERR_BUSY;
    }
    
    // 从最高层开始查找
    for (int level = MAX_SKIPLIST_LEVEL - 1; level >= 0; level--) {
        while (1) {
            // 原子读取next指针
            ppdb_node_t* next = current->next[level];
            if (!next) break;
            
            // 获取读锁
            if (ppdb_sync_read_lock(next->lock) != PPDB_OK) {
                break;
            }
            
            // 检查删除标记
            if (ppdb_sync_counter_load(&next->is_deleted) || 
                ppdb_sync_counter_load(&next->is_garbage)) {
                ppdb_sync_read_unlock(next->lock);
                current = next;
                continue;
            }
            
            // 比较key
            int cmp = memcmp(next->key->data, key->data, 
                           MIN(next->key->size, key->size));
            
            if (cmp > 0 || (cmp == 0 && next->key->size > key->size)) {
                ppdb_sync_read_unlock(next->lock);
                break;
            }
            
            if (cmp == 0 && next->key->size == key->size) {
                // 找到了匹配的key
                // 增加引用计数
                node_ref(next);
                
                // 分配value内存
                value->data = PPDB_ALIGNED_ALLOC(next->value->size);
                if (!value->data) {
                    node_unref(next);
                    ppdb_sync_read_unlock(next->lock);
                    ppdb_sync_read_unlock(base->storage.lock);
                    ppdb_sync_write_unlock(base->storage.lock);
                    return PPDB_ERR_OUT_OF_MEMORY;
                }

                // 复制数据
                memcpy(value->data, next->value->data, next->value->size);
                value->size = next->value->size;
                ppdb_sync_counter_init(&value->ref_count, 1);
                
                // 释放锁和引用
                ppdb_sync_read_unlock(next->lock);
                ppdb_sync_read_unlock(base->storage.lock);
                node_unref(next);
                
                ppdb_sync_counter_add(&base->metrics.get_hits, 1);
                return PPDB_OK;
            }
            
            current = next;
            ppdb_sync_read_unlock(next->lock);
        }
    }
    
    // 未找到
    ppdb_sync_read_unlock(base->storage.lock);
    return PPDB_ERR_NOT_FOUND;
}

// 刷盘操作（将在sharded实现中完成）
static ppdb_error_t memtable_flush(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;
    
    ppdb_error_t err = PPDB_OK;
    bool flush_lock_held = false;
    bool storage_lock_held = false;
    ppdb_base_t new_base = {0};
    bool new_base_initialized = false;

    // 获取刷盘锁
    if ((err = ppdb_sync_lock(base->mem.flush_lock)) != PPDB_OK) {
        goto cleanup;
    }
    flush_lock_held = true;

    // 初始化新的skiplist
    if ((err = skiplist_init(&new_base)) != PPDB_OK) {
        goto cleanup;
    }
    new_base_initialized = true;

    // 获取存储级写锁
    if ((err = ppdb_sync_write_lock(base->storage.lock)) != PPDB_OK) {
        goto cleanup;
    }
    storage_lock_held = true;

    // 遍历原skiplist，将数据写入磁盘并构建新的skiplist
    ppdb_node_t* current = base->storage.head->next[0];
    while (current) {
        // TODO: 将数据写入磁盘
        // 这里需要实现SSTable的写入逻辑
        
        // 将有效数据迁移到新的skiplist
        if (!ppdb_sync_counter_load(&current->is_deleted)) {
            err = skiplist_put(&new_base, current->key, current->value);
            if (err != PPDB_OK) {
                goto cleanup;
            }
        }
        current = current->next[0];
    }

    // 替换旧的skiplist（在存储级写锁保护下）
    ppdb_node_t* old_head = base->storage.head;
    base->storage.head = new_base.storage.head;
    new_base.storage.head = old_head;

    // 清理旧的skiplist
    ppdb_node_t* cleanup_node = old_head;
    while (cleanup_node) {
        ppdb_node_t* next = cleanup_node->next[0];
        node_unref(cleanup_node);
        cleanup_node = next;
    }

    // 重置内存使用量
    ppdb_sync_counter_store(&base->mem.used, sizeof(ppdb_node_t));

cleanup:
    if (storage_lock_held) {
        ppdb_sync_write_unlock(base->storage.lock);
    }
    if (err != PPDB_OK && new_base_initialized) {
        skiplist_destroy(&new_base);
    }
    if (flush_lock_held) {
        ppdb_sync_unlock(base->mem.flush_lock);
    }
    return err;
}

// 统一的计数器初始化函数
static ppdb_error_t init_metrics(ppdb_metrics_t* metrics) {
    if (!metrics) return PPDB_ERR_NULL_POINTER;
    
    ppdb_error_t err = PPDB_OK;
    err = ppdb_sync_counter_init(&metrics->get_count, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->get_hits, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->put_count, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->remove_count, 0);
    if (err != PPDB_OK) return err;
    
    return PPDB_OK;
}

// 统一的清理函数
static void cleanup_base(ppdb_base_t* base) {
    if (!base) return;
    
    if (base->array.ptrs) {
        for (uint32_t i = 0; i < base->array.count; i++) {
            if (base->array.ptrs[i]) {
                memtable_destroy(base->array.ptrs[i]);
                PPDB_ALIGNED_FREE(base->array.ptrs[i]);
            }
        }
        PPDB_ALIGNED_FREE(base->array.ptrs);
        base->array.ptrs = NULL;
        base->array.count = 0;
    }
    
    if (base->storage.head) {
        skiplist_destroy(base);
        base->storage.head = NULL;
    }
    
    if (base->mem.flush_lock) {
        ppdb_sync_destroy(base->mem.flush_lock);
        base->mem.flush_lock = NULL;
    }
}

static ppdb_error_t sharded_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    ppdb_error_t err = PPDB_OK;
    
    // 初始化基本字段
    base->type = PPDB_TYPE_SHARDED;
    base->array.count = DEFAULT_SHARD_COUNT;
    base->array.ptrs = NULL;
    
    // 分配分片数组
    base->array.ptrs = PPDB_ALIGNED_ALLOC(base->array.count * sizeof(ppdb_base_t*));
    if (!base->array.ptrs) {
        err = PPDB_ERR_OUT_OF_MEMORY;
        goto cleanup;
    }
    memset(base->array.ptrs, 0, base->array.count * sizeof(ppdb_base_t*));

    // 初始化每个分片
    for (uint32_t i = 0; i < base->array.count; i++) {
        base->array.ptrs[i] = PPDB_ALIGNED_ALLOC(sizeof(ppdb_base_t));
        if (!base->array.ptrs[i]) {
            err = PPDB_ERR_OUT_OF_MEMORY;
            goto cleanup;
        }
        memset(base->array.ptrs[i], 0, sizeof(ppdb_base_t));

        if ((err = memtable_init(base->array.ptrs[i])) != PPDB_OK) {
            goto cleanup;
        }
    }

    // 初始化统计信息
    if ((err = init_metrics(&base->metrics)) != PPDB_OK) {
        goto cleanup;
    }

    return PPDB_OK;

cleanup:
    cleanup_base(base);
    return err;
}

static ppdb_error_t sharded_destroy(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;
    
    // 获取写锁
    if (ppdb_sync_write_lock(base->storage.lock) == PPDB_OK) {
        // 销毁所有分片
        for (uint32_t i = 0; i < base->array.count; i++) {
            if (base->array.ptrs[i]) {
                skiplist_destroy(base->array.ptrs[i]);
                PPDB_ALIGNED_FREE(base->array.ptrs[i]);
            }
        }
        PPDB_ALIGNED_FREE(base->array.ptrs);
        base->array.ptrs = NULL;
        base->array.count = 0;
        
        // 释放写锁
        ppdb_sync_write_unlock(base->storage.lock);
    }
    
    // 销毁锁
    ppdb_sync_destroy(base->storage.lock);
    return PPDB_OK;
}

// 计算key的分片索引
static uint32_t get_shard_index(const ppdb_key_t* key, uint32_t shard_count) {
    // 使用MurmurHash3计算哈希值
    uint32_t hash = 0;
    uint32_t seed = 0x12345678;
    MurmurHash3_x86_32(key->data, key->size, seed, &hash);
    return hash % shard_count;
}

static ppdb_error_t sharded_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;

    // 计算分片索引
    uint32_t index = get_shard_index(key, base->array.count);
    ppdb_base_t* shard = base->array.ptrs[index];
    if (!shard) return PPDB_ERR_NOT_FOUND;

    // 在对应分片中查找
    return memtable_get(shard, key, value);
}

static ppdb_error_t sharded_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;

    // 计算分片索引
    uint32_t index = get_shard_index(key, base->array.count);
    ppdb_base_t* shard = base->array.ptrs[index];
    if (!shard) return PPDB_ERR_NOT_INITIALIZED;

    // 在对应分片中插入
    ppdb_error_t err = memtable_put(shard, key, value);
    if (err == PPDB_ERR_BUSY) {
        // 如果分片memtable已满，触发刷盘
        err = memtable_flush(shard);
        if (err != PPDB_OK) return err;
        
        // 重试插入
        err = memtable_put(shard, key, value);
    }
    return err;
}

// KV存储操作实现
static ppdb_error_t kvstore_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    ppdb_error_t err = PPDB_OK;
    
    // 初始化基本字段
    base->type = PPDB_TYPE_KVSTORE;
    
    // 创建分片存储
    if ((err = sharded_init(base)) != PPDB_OK) {
        goto cleanup;
    }

    // 初始化统计信息（已经在sharded_init中完成）
    return PPDB_OK;

cleanup:
    cleanup_base(base);
    return err;
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

// 存储同步操作实现
ppdb_error_t ppdb_storage_sync(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    switch (base->type) {
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

    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
            return PPDB_OK;  // 基础跳表不需要刷新
        case PPDB_TYPE_MEMTABLE:
            return memtable_flush(base);
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

    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
            return PPDB_OK;  // 基础跳表不需要压缩
        case PPDB_TYPE_MEMTABLE:
            return memtable_flush(base);  // 对memtable来说，压缩就是刷盘
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
ppdb_error_t ppdb_storage_get_stats(ppdb_base_t* base, ppdb_metrics_t* stats) {
    if (!base || !stats) {
        return PPDB_ERR_NULL_POINTER;
    }

    ppdb_error_t err = ppdb_sync_read_lock(base->storage.lock);
    if (err != PPDB_OK) {
        return err;
    }

    // 复制统计信息
    *stats = base->metrics;

    ppdb_sync_read_unlock(base->storage.lock);
    return PPDB_OK;
}

// 存储创建操作实现
ppdb_error_t ppdb_skiplist_create(ppdb_base_t* base, const ppdb_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_NULL_POINTER;
    }
    base->type = PPDB_TYPE_SKIPLIST;
    return skiplist_init(base);
}

ppdb_error_t ppdb_memtable_create(ppdb_base_t* base, const ppdb_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_NULL_POINTER;
    }
    base->type = PPDB_TYPE_MEMTABLE;
    return memtable_init(base);
}

ppdb_error_t ppdb_sharded_create(ppdb_base_t* base, const ppdb_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_NULL_POINTER;
    }
    base->type = PPDB_TYPE_SHARDED;
    return sharded_init(base);
}

ppdb_error_t ppdb_kvstore_create(ppdb_base_t* base, const ppdb_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_NULL_POINTER;
    }
    base->type = PPDB_TYPE_KVSTORE;
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
    if (!base) return;
    
    // 获取写锁
    if (ppdb_sync_write_lock(base->storage.lock) == PPDB_OK) {
        // 销毁所有分片
        for (uint32_t i = 0; i < base->array.count; i++) {
            if (base->array.ptrs[i]) {
                skiplist_destroy(base->array.ptrs[i]);
                PPDB_ALIGNED_FREE(base->array.ptrs[i]);
            }
        }
        PPDB_ALIGNED_FREE(base->array.ptrs);
        base->array.ptrs = NULL;
        base->array.count = 0;
        
        // 释放写锁
        ppdb_sync_write_unlock(base->storage.lock);
    }
    
    // 销毁锁
    ppdb_sync_destroy(base->storage.lock);
}

void ppdb_kvstore_destroy(ppdb_base_t* base) {
    if (!base) return;
    
    // 销毁所有分片
    if (base->array.ptrs) {
        for (uint32_t i = 0; i < base->array.count; i++) {
            if (base->array.ptrs[i]) {
                memtable_destroy(base->array.ptrs[i]);
                PPDB_ALIGNED_FREE(base->array.ptrs[i]);
            }
        }
        PPDB_ALIGNED_FREE(base->array.ptrs);
    }
    
    // 销毁锁
    if (base->storage.lock) {
        ppdb_sync_destroy(base->storage.lock);
    }
}

//-----------------------------------------------------------------------------
// 内存KV存储实现
//-----------------------------------------------------------------------------

static ppdb_error_t memkv_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;
    
    // 初始化分片存储
    base->array.count = DEFAULT_SHARD_COUNT;
    base->array.ptrs = calloc(DEFAULT_SHARD_COUNT, sizeof(void*));
    if (!base->array.ptrs) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    
    // 初始化每个分片为跳表
    for (uint32_t i = 0; i < base->array.count; i++) {
        ppdb_base_t* shard;
        ppdb_error_t err = ppdb_create(PPDB_TYPE_SKIPLIST, &shard);
        if (err != PPDB_OK) {
            // 清理已创建的分片
            for (uint32_t j = 0; j < i; j++) {
                ppdb_destroy(base->array.ptrs[j]);
            }
            free(base->array.ptrs);
            return err;
        }
        base->array.ptrs[i] = shard;
    }
    
    // 初始化统计信息
    ppdb_sync_counter_init(&base->metrics.get_count, 0);
    ppdb_sync_counter_init(&base->metrics.get_hits, 0);
    ppdb_sync_counter_init(&base->metrics.put_count, 0);
    ppdb_sync_counter_init(&base->metrics.remove_count, 0);
    
    return PPDB_OK;
}

static ppdb_error_t memkv_destroy(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;
    
    // 清理所有分片
    if (base->array.ptrs) {
        for (uint32_t i = 0; i < base->array.count; i++) {
            if (base->array.ptrs[i]) {
                ppdb_destroy(base->array.ptrs[i]);
            }
        }
        PPDB_ALIGNED_FREE(base->array.ptrs);
    }
    
    return PPDB_OK;
}

static uint32_t memkv_get_shard(const ppdb_key_t* key, uint32_t shard_count) {
    // 简单的哈希函数，用于分片选择
    uint32_t hash = 0;
    const uint8_t* data = key->data;
    for (size_t i = 0; i < key->size; i++) {
        hash = hash * 31 + data[i];
    }
    return hash % shard_count;
}

static ppdb_error_t memkv_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;
    
    // 获取分片索引
    uint32_t shard = memkv_get_shard(key, base->array.count);
    ppdb_base_t* shard_base = base->array.ptrs[shard];
    
    // 在分片中查找
    ppdb_error_t err = ppdb_get(shard_base, key, value);
    if (err == PPDB_OK) {
        ppdb_sync_counter_add(&base->metrics.get_hits, 1);
    }
    ppdb_sync_counter_add(&base->metrics.get_count, 1);
    
    return err;
}

static ppdb_error_t memkv_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;
    
    // 获取分片索引
    uint32_t shard = memkv_get_shard(key, base->array.count);
    ppdb_base_t* shard_base = base->array.ptrs[shard];
    
    // 在分片中写入
    ppdb_error_t err = ppdb_put(shard_base, key, value);
    if (err == PPDB_OK) {
        ppdb_sync_counter_add(&base->metrics.put_count, 1);
    }
    
    return err;
}

static ppdb_error_t memkv_remove(ppdb_base_t* base, const ppdb_key_t* key) {
    if (!base || !key) return PPDB_ERR_NULL_POINTER;
    
    // 获取分片索引
    uint32_t shard = memkv_get_shard(key, base->array.count);
    ppdb_base_t* shard_base = base->array.ptrs[shard];
    
    // 在分片中删除
    ppdb_error_t err = ppdb_remove(shard_base, key);
    if (err == PPDB_OK) {
        ppdb_sync_counter_add(&base->metrics.remove_count, 1);
    }
    
    return err;
}

// 跳表迭代器结构体
typedef struct skiplist_iterator {
    ppdb_base_t* base;          // 数据库实例
    ppdb_node_t* current;       // 当前节点
    ppdb_sync_t* lock;          // 迭代器锁
    bool is_valid;              // 迭代器是否有效
    ppdb_sync_counter_t ref_count;  // 引用计数
} skiplist_iterator_t;

// 创建跳表迭代器
static ppdb_error_t skiplist_iterator_create(ppdb_base_t* base, void** iter) {
    if (!base || !iter) return PPDB_ERR_NULL_POINTER;

    // 分配迭代器内存
    skiplist_iterator_t* it = PPDB_ALIGNED_ALLOC(sizeof(skiplist_iterator_t));
    if (!it) return PPDB_ERR_OUT_OF_MEMORY;

    // 初始化迭代器
    it->base = base;
    it->current = base->storage.head;  // 从头节点开始
    it->is_valid = true;
    ppdb_sync_counter_init(&it->ref_count, 1);

    // 创建迭代器锁
    if (ppdb_sync_create(&it->lock, &(ppdb_sync_config_t){
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = base->config.use_lockfree,
        .max_readers = 1,
        .backoff_us = 1,
        .max_retries = 100
    }) != PPDB_OK) {
        PPDB_ALIGNED_FREE(it);
        return PPDB_ERR_LOCK_FAILED;
    }

    *iter = it;
    return PPDB_OK;
}

// 获取下一个有效节点
static ppdb_error_t skiplist_iterator_next(void* iter, ppdb_key_t* key, ppdb_value_t* value) {
    if (!iter || !key || !value) return PPDB_ERR_NULL_POINTER;
    
    skiplist_iterator_t* it = (skiplist_iterator_t*)iter;
    if (!it->is_valid) return PPDB_ERR_NOT_FOUND;

    // 获取迭代器写锁
    ppdb_error_t err = ppdb_sync_write_lock(it->lock);
    if (err != PPDB_OK) return err;

    // 获取存储读锁
    err = ppdb_sync_read_lock(it->base->storage.lock);
    if (err != PPDB_OK) {
        ppdb_sync_write_unlock(it->lock);
        return err;
    }

    // 查找下一个有效节点
    while (it->current) {
        ppdb_node_t* next = it->current->next[0];
        if (!next) {
            it->is_valid = false;
            ppdb_sync_read_unlock(it->base->storage.lock);
            ppdb_sync_write_unlock(it->lock);
            return PPDB_ERR_NOT_FOUND;
        }

        // 获取节点读锁
        if (ppdb_sync_read_lock(next->lock) != PPDB_OK) {
            it->current = next;
            continue;
        }

        // 检查节点状态
        if (ppdb_sync_counter_load(&next->is_deleted) || 
            ppdb_sync_counter_load(&next->is_garbage)) {
            ppdb_sync_read_unlock(next->lock);
            it->current = next;
            continue;
        }

        // 复制key
        key->data = PPDB_ALIGNED_ALLOC(next->key->size);
        if (!key->data) {
            ppdb_sync_read_unlock(next->lock);
            ppdb_sync_read_unlock(it->base->storage.lock);
            ppdb_sync_write_unlock(it->lock);
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        memcpy(key->data, next->key->data, next->key->size);
        key->size = next->key->size;
        ppdb_sync_counter_init(&key->ref_count, 1);

        // 复制value
        value->data = PPDB_ALIGNED_ALLOC(next->value->size);
        if (!value->data) {
            PPDB_ALIGNED_FREE(key->data);
            ppdb_sync_read_unlock(next->lock);
            ppdb_sync_read_unlock(it->base->storage.lock);
            ppdb_sync_write_unlock(it->lock);
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        memcpy(value->data, next->value->data, next->value->size);
        value->size = next->value->size;
        ppdb_sync_counter_init(&value->ref_count, 1);

        // 更新当前节点
        it->current = next;
        ppdb_sync_read_unlock(next->lock);
        ppdb_sync_read_unlock(it->base->storage.lock);
        ppdb_sync_write_unlock(it->lock);
        return PPDB_OK;
    }

    it->is_valid = false;
    ppdb_sync_read_unlock(it->base->storage.lock);
    ppdb_sync_write_unlock(it->lock);
    return PPDB_ERR_NOT_FOUND;
}

// 销毁迭代器
static void skiplist_iterator_destroy(void* iter) {
    if (!iter) return;
    
    skiplist_iterator_t* it = (skiplist_iterator_t*)iter;
    
    // 获取写锁
    if (ppdb_sync_write_lock(it->lock) == PPDB_OK) {
        it->is_valid = false;
        ppdb_sync_write_unlock(it->lock);
    }
    
    // 减少引用计数
    if (ppdb_sync_counter_sub(&it->ref_count, 1) == 1) {
        ppdb_sync_destroy(it->lock);
        PPDB_ALIGNED_FREE(it);
    }
}

// 注册迭代器接口
ppdb_error_t ppdb_iterator_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    // 分配高级操作接口
    if (!base->advance) {
        base->advance = PPDB_ALIGNED_ALLOC(sizeof(ppdb_advance_ops_t));
        if (!base->advance) return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 注册迭代器接口
    base->advance->iterator = skiplist_iterator_create;
    base->advance->next = skiplist_iterator_next;
    base->advance->iterator_destroy = skiplist_iterator_destroy;

    return PPDB_OK;
}

// 基本操作函数实现
ppdb_error_t ppdb_create(ppdb_type_t type, ppdb_base_t** base) {
    if (!base) return PPDB_ERR_NULL_POINTER;
    
    *base = PPDB_ALIGNED_ALLOC(sizeof(ppdb_base_t));
    if (!*base) return PPDB_ERR_OUT_OF_MEMORY;
    
    memset(*base, 0, sizeof(ppdb_base_t));
    (*base)->type = type;
    
    // 初始化配置
    const char* test_mode = getenv("PPDB_SYNC_MODE");
    (*base)->config.type = type;
    (*base)->config.use_lockfree = (test_mode && strcmp(test_mode, "lockfree") == 0);
    (*base)->config.memtable_size = DEFAULT_MEMTABLE_SIZE;
    (*base)->config.shard_count = DEFAULT_SHARD_COUNT;
    
    ppdb_error_t err;
    switch (type) {
        case PPDB_TYPE_SKIPLIST:
            err = skiplist_init(*base);
            break;
        case PPDB_TYPE_MEMTABLE:
            err = memtable_init(*base);
            break;
        case PPDB_TYPE_SHARDED:
            err = sharded_init(*base);
            break;
        case PPDB_TYPE_KVSTORE:
            err = kvstore_init(*base);
            break;
        default:
            err = PPDB_ERR_INVALID_TYPE;
    }
    
    if (err != PPDB_OK) {
        PPDB_ALIGNED_FREE(*base);
        *base = NULL;
    }
    
    return err;
}

void ppdb_destroy(ppdb_base_t* base) {
    if (!base) return;
    
    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
            skiplist_destroy(base);
            break;
        case PPDB_TYPE_MEMTABLE:
            memtable_destroy(base);
            break;
        case PPDB_TYPE_SHARDED:
            sharded_destroy(base);
            break;
        case PPDB_TYPE_KVSTORE:
            kvstore_destroy(base);
            break;
        default:
            break;
    }
    
    PPDB_ALIGNED_FREE(base);
}

ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;
    
    ppdb_error_t err;
    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
            err = skiplist_get(base, key, value);
            break;
        case PPDB_TYPE_MEMTABLE:
            err = memtable_get(base, key, value);
            break;
        case PPDB_TYPE_SHARDED:
            err = sharded_get(base, key, value);
            break;
        case PPDB_TYPE_KVSTORE:
            err = kvstore_get(base, key, value);
            break;
        default:
            err = PPDB_ERR_INVALID_TYPE;
    }
    
    if (err == PPDB_OK) {
        ppdb_sync_counter_add(&base->metrics.get_hits, 1);
    }
    ppdb_sync_counter_add(&base->metrics.get_count, 1);
    
    return err;
}

ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;
    
    ppdb_error_t err;
    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
            err = skiplist_put(base, key, value);
            break;
        case PPDB_TYPE_MEMTABLE:
            err = memtable_put(base, key, value);
            break;
        case PPDB_TYPE_SHARDED:
            err = sharded_put(base, key, value);
            break;
        case PPDB_TYPE_KVSTORE:
            err = kvstore_put(base, key, value);
            break;
        default:
            err = PPDB_ERR_INVALID_TYPE;
    }
    
    if (err == PPDB_OK) {
        ppdb_sync_counter_add(&base->metrics.put_count, 1);
    }
    
    return err;
}

ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key) {
    if (!base || !key) {
        return PPDB_ERR_NULL_POINTER;
    }

    ppdb_error_t err = PPDB_OK;
    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE: {
            if (!base->storage.head) {
                return PPDB_ERR_NOT_INITIALIZED;
            }

            // 获取写锁
            err = ppdb_sync_write_lock(base->storage.lock);
            if (err != PPDB_OK) {
                return err;
            }

            // 查找节点
            ppdb_node_t* current = base->storage.head;
            ppdb_node_t* update[MAX_SKIPLIST_LEVEL] = {NULL};
            ppdb_node_t* target = NULL;

            // 从最高层开始查找
            for (int level = MAX_SKIPLIST_LEVEL - 1; level >= 0; level--) {
                while (current->next[level]) {
                    ppdb_node_t* next = current->next[level];
                    if (ppdb_sync_counter_load(&next->is_deleted)) {
                        current = next;
                        continue;
                    }

                    int cmp = memcmp(next->key->data, key->data, 
                                   MIN(next->key->size, key->size));
                    if (cmp > 0 || (cmp == 0 && next->key->size > key->size)) {
                        break;
                    }

                    if (cmp == 0 && next->key->size == key->size) {
                        target = next;
                    }

                    current = next;
                }
                update[level] = current;
            }

            // 如果没有找到目标节点
            if (!target) {
                ppdb_sync_write_unlock(base->storage.lock);
                return PPDB_ERR_NOT_FOUND;
            }

            // 获取目标节点的写锁
            err = ppdb_sync_write_lock(target->lock);
            if (err != PPDB_OK) {
                ppdb_sync_write_unlock(base->storage.lock);
                return err;
            }

            // 再次检查删除标记
            if (ppdb_sync_counter_load(&target->is_deleted)) {
                ppdb_sync_write_unlock(target->lock);
                ppdb_sync_write_unlock(base->storage.lock);
                return PPDB_ERR_NOT_FOUND;
            }

            // 标记为已删除
            ppdb_sync_counter_store(&target->is_deleted, 1);

            // 更新所有层级的指针
            uint32_t target_height = node_get_height(target);
            for (uint32_t i = 0; i < target_height; i++) {
                if (update[i] && update[i]->next[i] == target) {
                    update[i]->next[i] = target->next[i];
                }
            }

            // 减少引用计数
            node_unref(target);

            // 释放锁
            ppdb_sync_write_unlock(target->lock);
            ppdb_sync_write_unlock(base->storage.lock);

            // 更新指标
            ppdb_sync_counter_add(&base->metrics.remove_count, 1);
            return PPDB_OK;
        }
        case PPDB_TYPE_SHARDED:
        case PPDB_TYPE_KVSTORE: {
            if (!base->array.ptrs) {
                return PPDB_ERR_NOT_INITIALIZED;
            }
            uint32_t index = get_shard_index(key, base->array.count);
            ppdb_base_t* shard = base->array.ptrs[index];
            if (!shard) {
                return PPDB_ERR_NOT_INITIALIZED;
            }
            err = ppdb_remove(shard, key);
            if (err == PPDB_OK) {
                ppdb_sync_counter_add(&base->metrics.remove_count, 1);
            }
            break;
        }
        default:
            return PPDB_ERR_INVALID_TYPE;
    }

    return err;
}
