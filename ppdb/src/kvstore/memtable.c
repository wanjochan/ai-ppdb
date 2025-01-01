#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "ppdb/ppdb_logger.h"
#include "kvstore/internal/kvstore_fs.h"
#include "kvstore/internal/skiplist.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/metrics.h"

// 跳表节点大小估计
#define PPDB_SKIPLIST_NODE_SIZE 64

// 包装器函数
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table) {
    return ppdb_memtable_create_basic(size_limit, table);
}

void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    ppdb_memtable_destroy_basic(table);
}

ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              const void* value, size_t value_len) {
    return ppdb_memtable_put_basic(table, key, key_len, value, value_len);
}

ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              void** value, size_t* value_len) {
    return ppdb_memtable_get_basic(table, key, key_len, value, value_len);
}

ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const void* key, size_t key_len) {
    return ppdb_memtable_delete_basic(table, key, key_len);
}

size_t ppdb_memtable_size(ppdb_memtable_t* table) {
    return ppdb_memtable_size_basic(table);
}

size_t ppdb_memtable_max_size(ppdb_memtable_t* table) {
    return ppdb_memtable_max_size_basic(table);
}

bool ppdb_memtable_is_immutable(ppdb_memtable_t* table) {
    return ppdb_memtable_is_immutable_basic(table);
}

void ppdb_memtable_set_immutable(ppdb_memtable_t* table) {
    ppdb_memtable_set_immutable_basic(table);
}

const ppdb_metrics_t* ppdb_memtable_get_metrics(ppdb_memtable_t* table) {
    return ppdb_memtable_get_metrics_basic(table);
}

// 获取分片索引
static size_t ppdb_memtable_get_shard_index(const void* key, size_t key_len, size_t shard_count);

// 分片内存表操作
ppdb_error_t ppdb_memtable_create_sharded_basic(size_t size_limit, ppdb_memtable_t** table) {
    if (!table) {
        return PPDB_ERR_NULL_POINTER;
    }

    // 如果没有指定大小限制，使用默认值
    if (size_limit == 0) {
        size_limit = 1024 * 1024 * 1024; // 1GB
    }

    // 初始化同步配置
    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000,
        .use_lockfree = false,
        .stripe_count = 8,
        .backoff_us = 1,
        .enable_ref_count = false
    };

    // 分配内存表结构
    ppdb_memtable_t* new_table = calloc(1, sizeof(ppdb_memtable_t));
    if (!new_table) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 分配分片数组
    new_table->shards = calloc(8, sizeof(ppdb_memtable_shard_t));
    if (!new_table->shards) {
        free(new_table);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化内存表
    new_table->type = PPDB_MEMTABLE_SHARDED;
    new_table->size_limit = size_limit;
    atomic_init(&new_table->current_size, 0);
    new_table->shard_count = 8;  // 默认8个分片
    new_table->is_immutable = false;

    // 初始化性能指标
    atomic_init(&new_table->metrics.put_count, 0);
    atomic_init(&new_table->metrics.get_count, 0);
    atomic_init(&new_table->metrics.delete_count, 0);
    atomic_init(&new_table->metrics.total_ops, 0);
    atomic_init(&new_table->metrics.total_latency, 0);
    atomic_init(&new_table->metrics.total_latency_us, 0);
    atomic_init(&new_table->metrics.max_latency_us, 0);
    atomic_init(&new_table->metrics.min_latency_us, UINT64_MAX);
    atomic_init(&new_table->metrics.total_bytes, 0);
    atomic_init(&new_table->metrics.total_keys, 0);
    atomic_init(&new_table->metrics.total_values, 0);
    atomic_init(&new_table->metrics.bytes_written, 0);
    atomic_init(&new_table->metrics.bytes_read, 0);
    atomic_init(&new_table->metrics.get_miss_count, 0);

    // 初始化每个分片
    for (size_t i = 0; i < new_table->shard_count; i++) {
        // 初始化同步原语
        ppdb_error_t err = ppdb_sync_init(&new_table->shards[i].sync, &sync_config);
        if (err != PPDB_OK) {
            for (size_t j = 0; j < i; j++) {
                ppdb_skiplist_destroy(new_table->shards[j].skiplist);
                ppdb_sync_destroy(&new_table->shards[j].sync);
            }
            free(new_table->shards);
            free(new_table);
            return err;
        }

        // 创建跳表
        ppdb_skiplist_t* skiplist = NULL;
        if ((err = ppdb_skiplist_create(&skiplist,
                                PPDB_SKIPLIST_MAX_LEVEL,
                                ppdb_skiplist_default_compare,
                                &sync_config)) != PPDB_OK) {
            printf("Error creating skiplist: %d\n", err);
            ppdb_sync_destroy(&new_table->shards[i].sync);
            for (size_t j = 0; j < i; j++) {
                ppdb_skiplist_destroy(new_table->shards[j].skiplist);
                ppdb_sync_destroy(&new_table->shards[j].sync);
            }
            free(new_table->shards);
            free(new_table);
            return err;
        }
        new_table->shards[i].skiplist = skiplist;

        // 初始化分片大小
        atomic_init(&new_table->shards[i].size, 0);
    }

    *table = new_table;
    return PPDB_OK;
}

void ppdb_memtable_destroy_sharded(ppdb_memtable_t* table) {
    if (!table) {
        return;
    }

    // 销毁每个分片
    if (table->shards) {
        for (size_t i = 0; i < table->shard_count; i++) {
            ppdb_skiplist_destroy(table->shards[i].skiplist);
            ppdb_sync_destroy(&table->shards[i].sync);
        }
        free(table->shards);
    }

    // 释放内存表结构
    free(table);
}

ppdb_error_t ppdb_memtable_put_sharded_basic(ppdb_memtable_t* table,
                                           const void* key, size_t key_len,
                                           const void* value, size_t value_len) {
    if (!table || !table->shards || !key || !value) {
        return PPDB_ERR_NULL_POINTER;
    }
    if (key_len == 0 || value_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }
    if (table->is_immutable) {
        return PPDB_ERR_IMMUTABLE;
    }

    // 计算所需总空间
    size_t total_size = key_len + value_len + PPDB_SKIPLIST_NODE_SIZE;
    
    // 检查是否超过大小限制
    size_t current = atomic_load(&table->current_size);
    if (current + total_size > table->size_limit) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 计算分片索引
    size_t shard_index = ppdb_memtable_get_shard_index(key, key_len, table->shard_count);
    ppdb_memtable_shard_t* shard = &table->shards[shard_index];

    ppdb_error_t err;
    if ((err = ppdb_sync_lock(&shard->sync)) != PPDB_OK) {
        return err;
    }

    // 写入键值对
    err = ppdb_skiplist_put(shard->skiplist, key, key_len, value, value_len);
    if (err == PPDB_OK) {
        atomic_fetch_add(&shard->size, total_size);
        atomic_fetch_add(&table->current_size, total_size);
        table->metrics.put_count++;
        table->metrics.bytes_written += total_size;
    }

    ppdb_sync_unlock(&shard->sync);
    return err;
}

ppdb_error_t ppdb_memtable_get_sharded_basic(ppdb_memtable_t* table,
                                           const void* key, size_t key_len,
                                           void** value, size_t* value_len) {
    if (!table || !table->shards || !key || !value || !value_len) {
        return PPDB_ERR_NULL_POINTER;
    }
    if (key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 计算分片索引
    size_t shard_index = ppdb_memtable_get_shard_index(key, key_len, table->shard_count);
    ppdb_memtable_shard_t* shard = &table->shards[shard_index];

    ppdb_error_t err;
    if ((err = ppdb_sync_lock(&shard->sync)) != PPDB_OK) {
        return err;
    }

    // 读取键值对
    err = ppdb_skiplist_get(shard->skiplist, key, key_len, value, value_len);
    if (err == PPDB_OK) {
        table->metrics.get_count++;
        table->metrics.bytes_read += key_len + *value_len;
    } else {
        table->metrics.get_miss_count++;
    }

    ppdb_sync_unlock(&shard->sync);
    return err;
}

ppdb_error_t ppdb_memtable_delete_sharded_basic(ppdb_memtable_t* table,
                                              const void* key, size_t key_len) {
    if (!table || !table->shards || !key) {
        return PPDB_ERR_NULL_POINTER;
    }
    if (key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }
    if (table->is_immutable) {
        return PPDB_ERR_IMMUTABLE;
    }

    // 计算分片索引
    size_t shard_index = ppdb_memtable_get_shard_index(key, key_len, table->shard_count);
    ppdb_memtable_shard_t* shard = &table->shards[shard_index];

    ppdb_error_t err;
    if ((err = ppdb_sync_lock(&shard->sync)) != PPDB_OK) {
        return err;
    }

    // 删除键值对
    err = ppdb_skiplist_delete(shard->skiplist, key, key_len);
    if (err == PPDB_OK) {
        table->metrics.delete_count++;
    }

    ppdb_sync_unlock(&shard->sync);
    return err;
}

// 获取分片索引
static size_t ppdb_memtable_get_shard_index(const void* key, size_t key_len, size_t shard_count) {
    // 简单的哈希函数
    size_t hash = 0;
    const unsigned char* bytes = (const unsigned char*)key;
    for (size_t i = 0; i < key_len; i++) {
        hash = hash * 31 + bytes[i];
    }
    return hash % shard_count;
}

// 创建内存表
ppdb_error_t ppdb_memtable_create_basic(size_t size_limit, ppdb_memtable_t** table) {
    if (!table) {
        return PPDB_ERR_NULL_POINTER;
    }
    if (size_limit == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 初始化同步配置
    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000,
        .use_lockfree = false,
        .stripe_count = 8,
        .backoff_us = 1,
        .enable_ref_count = false
    };

    // 分配内存表结构
    ppdb_memtable_t* new_table = calloc(1, sizeof(ppdb_memtable_t));
    if (!new_table) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 分配基础内存表结构
    new_table->basic = calloc(1, sizeof(ppdb_memtable_basic_t));
    if (!new_table->basic) {
        free(new_table);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化基础结构
    new_table->type = PPDB_MEMTABLE_BASIC;
    new_table->size_limit = size_limit;
    atomic_init(&new_table->current_size, 0);
    new_table->shard_count = 1;
    new_table->is_immutable = false;

    // 初始化基础内存表
    new_table->basic->skiplist = NULL;
    new_table->basic->used = 0;
    new_table->basic->size = size_limit;

    // 初始化性能指标
    atomic_init(&new_table->metrics.put_count, 0);
    atomic_init(&new_table->metrics.get_count, 0);
    atomic_init(&new_table->metrics.delete_count, 0);
    atomic_init(&new_table->metrics.total_ops, 0);
    atomic_init(&new_table->metrics.total_latency, 0);
    atomic_init(&new_table->metrics.total_latency_us, 0);
    atomic_init(&new_table->metrics.max_latency_us, 0);
    atomic_init(&new_table->metrics.min_latency_us, UINT64_MAX);
    atomic_init(&new_table->metrics.total_bytes, 0);
    atomic_init(&new_table->metrics.total_keys, 0);
    atomic_init(&new_table->metrics.total_values, 0);
    atomic_init(&new_table->metrics.bytes_written, 0);
    atomic_init(&new_table->metrics.bytes_read, 0);
    atomic_init(&new_table->metrics.get_miss_count, 0);

    // 初始化同步原语
    ppdb_error_t err = ppdb_sync_init(&new_table->basic->sync, &sync_config);
    if (err != PPDB_OK) {
        free(new_table->basic);
        free(new_table);
        return err;
    }

    // 创建跳表
    ppdb_skiplist_t* skiplist = NULL;
    if ((err = ppdb_skiplist_create(&skiplist,
                             PPDB_SKIPLIST_MAX_LEVEL,
                             ppdb_skiplist_default_compare,
                             &sync_config)) != PPDB_OK) {
        printf("Error creating skiplist: %d\n", err);
        ppdb_sync_destroy(&new_table->basic->sync);
        free(new_table->basic);
        free(new_table);
        return err;
    }
    new_table->basic->skiplist = skiplist;

    *table = new_table;
    return PPDB_OK;
}

// 销毁内存表
void ppdb_memtable_destroy_basic(ppdb_memtable_t* table) {
    if (!table) {
        return;
    }
    
    // 销毁基础结构
    if (table->basic) {
        // 销毁跳表
        if (table->basic->skiplist) {
            ppdb_skiplist_destroy(table->basic->skiplist);
            table->basic->skiplist = NULL;
        }
        
        // 销毁同步原语
        ppdb_sync_destroy(&table->basic->sync);
        
        // 释放基础结构
        free(table->basic);
        table->basic = NULL;
    }
    
    // 释放内存表结构
    free(table);
}

// 写入键值对
ppdb_error_t ppdb_memtable_put_basic(ppdb_memtable_t* table,
                                    const void* key, size_t key_len,
                                    const void* value, size_t value_len) {
    if (!table || !table->basic || !key || !value) {
        return PPDB_ERR_NULL_POINTER;
    }
    if (key_len == 0 || value_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }
    if (table->is_immutable) {
        return PPDB_ERR_IMMUTABLE;
    }

    // 计算所需总空间
    size_t total_size = key_len + value_len + PPDB_SKIPLIST_NODE_SIZE;
    
    // 检查是否超过大小限制
    size_t current = atomic_load(&table->current_size);
    if (current + total_size > table->size_limit) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    ppdb_error_t err;
    if ((err = ppdb_sync_lock(&table->basic->sync)) != PPDB_OK) {
        return err;
    }

    // 再次检查大小（在锁内）
    if (table->basic->used + total_size > table->basic->size) {
        ppdb_sync_unlock(&table->basic->sync);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    err = ppdb_skiplist_put(table->basic->skiplist,
                           key, key_len,
                           value, value_len);
    if (err == PPDB_OK) {
        table->basic->used += total_size;
        atomic_fetch_add(&table->current_size, total_size);
        table->metrics.put_count++;
        table->metrics.bytes_written += total_size;
    }

    ppdb_sync_unlock(&table->basic->sync);
    return err;
}

// 读取键值对
ppdb_error_t ppdb_memtable_get_basic(ppdb_memtable_t* table,
                                    const void* key, size_t key_len,
                                    void** value, size_t* value_len) {
    if (!table || !table->basic || !key || !value_len) {
        return PPDB_ERR_NULL_POINTER;
    }
    if (key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err;
    if ((err = ppdb_sync_lock(&table->basic->sync)) != PPDB_OK) {
        return err;
    }

    err = ppdb_skiplist_get(table->basic->skiplist,
                           key, key_len,
                           value, value_len);
    
    if (err == PPDB_OK) {
        table->metrics.get_count++;
        table->metrics.bytes_read += *value_len;
    } else if (err == PPDB_ERR_NOT_FOUND) {
        table->metrics.get_miss_count++;
    }

    ppdb_sync_unlock(&table->basic->sync);
    return err;
}

// 删除键值对
ppdb_error_t ppdb_memtable_delete_basic(ppdb_memtable_t* table,
                                       const void* key, size_t key_len) {
    if (!table || !table->basic || !key) {
        return PPDB_ERR_NULL_POINTER;
    }
    if (key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }
    if (table->is_immutable) {
        return PPDB_ERR_IMMUTABLE;
    }

    ppdb_error_t err;
    if ((err = ppdb_sync_lock(&table->basic->sync)) != PPDB_OK) {
        return err;
    }

    err = ppdb_skiplist_delete(table->basic->skiplist,
                              key, key_len);
    if (err == PPDB_OK) {
        size_t node_size = key_len + PPDB_SKIPLIST_NODE_SIZE;
        table->basic->used -= node_size;
        atomic_fetch_sub(&table->current_size, node_size);
        table->metrics.delete_count++;
    }

    ppdb_sync_unlock(&table->basic->sync);
    return err;
}

// 获取当前大小
size_t ppdb_memtable_size_basic(ppdb_memtable_t* table) {
    return table && table->basic ? table->basic->used : 0;
}

// 获取最大大小
size_t ppdb_memtable_max_size_basic(ppdb_memtable_t* table) {
    return table && table->basic ? table->basic->size : 0;
}

// 是否不可变
bool ppdb_memtable_is_immutable_basic(ppdb_memtable_t* table) {
    return table ? table->is_immutable : false;
}

// 设置不可变
void ppdb_memtable_set_immutable_basic(ppdb_memtable_t* table) {
    if (table) {
        table->is_immutable = true;
    }
}

// 获取性能指标
const ppdb_metrics_t* ppdb_memtable_get_metrics_basic(ppdb_memtable_t* table) {
    return table ? &table->metrics : NULL;
}

// 创建迭代器
ppdb_error_t ppdb_memtable_iterator_create_basic(ppdb_memtable_t* table, ppdb_memtable_iterator_t** iter) {
    if (!table || !iter) {
        return PPDB_ERR_NULL_POINTER;
    }

    ppdb_memtable_iterator_t* new_iter = malloc(sizeof(ppdb_memtable_iterator_t));
    if (!new_iter) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    new_iter->table = table;
    new_iter->it = NULL;
    new_iter->valid = true;
    memset(&new_iter->current_pair, 0, sizeof(ppdb_kv_pair_t));

    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 100,
        .enable_ref_count = false
    };

    ppdb_error_t err = ppdb_skiplist_iterator_create(table->basic->skiplist,
                                                   &new_iter->it,
                                                   &sync_config);
    if (err != PPDB_OK) {
        free(new_iter);
        return err;
    }

    *iter = new_iter;
    return PPDB_OK;
}

// 迭代器移动到下一个元素
ppdb_error_t ppdb_memtable_iterator_next_basic(ppdb_memtable_iterator_t* iter,
                                              ppdb_kv_pair_t** pair) {
    if (!iter || !pair) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (!iter->it) {
        return PPDB_ERR_NOT_FOUND;
    }

    // 获取当前键值对
    ppdb_error_t err = ppdb_skiplist_iterator_get(iter->it, &iter->current_pair);
    if (err != PPDB_OK) {
        return err;
    }

    // 返回当前键值对
    *pair = &iter->current_pair;

    // 移动到下一个
    err = ppdb_skiplist_iterator_next(iter->it, &iter->current_pair);
    if (err != PPDB_OK) {
        iter->valid = false;
    }

    return PPDB_OK;
}

// 获取当前键值对
ppdb_error_t ppdb_memtable_iterator_get_basic(ppdb_memtable_iterator_t* iter,
                                             ppdb_kv_pair_t* pair) {
    if (!iter || !pair) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (!iter->it) {
        return PPDB_ERR_NOT_FOUND;
    }

    // 获取当前键值对
    ppdb_error_t err = ppdb_skiplist_iterator_get(iter->it, pair);
    if (err != PPDB_OK) {
        return err;
    }

    return PPDB_OK;
}

// 销毁迭代器
void ppdb_memtable_iterator_destroy_basic(ppdb_memtable_iterator_t* iter) {
    if (!iter) {
        return;
    }

    if (iter->it) {
        ppdb_skiplist_iterator_destroy(iter->it);
    }

    // 释放当前键值对
    if (iter->current_pair.key) {
        free(iter->current_pair.key);
    }
    if (iter->current_pair.value) {
        free(iter->current_pair.value);
    }

    free(iter);
}
