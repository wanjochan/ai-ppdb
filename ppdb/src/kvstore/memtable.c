#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_logger.h"
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

// 创建内存表
ppdb_error_t ppdb_memtable_create_basic(size_t size_limit, ppdb_memtable_t** table) {
    if (!table) return PPDB_ERR_NULL_POINTER;
    if (size_limit == 0) return PPDB_ERR_INVALID_ARG;

    // 分配内存表结构
    ppdb_memtable_t* new_table = calloc(1, sizeof(ppdb_memtable_t));
    if (!new_table) return PPDB_ERR_OUT_OF_MEMORY;

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

    // 初始化同步配置
    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 100,
        .enable_ref_count = false
    };

    // 初始化同步原语
    ppdb_error_t err = ppdb_sync_init(&new_table->basic->sync, &sync_config);
    if (err != PPDB_OK) {
        free(new_table->basic);
        free(new_table);
        return err;
    }

    // 创建跳表
    err = ppdb_skiplist_create(&new_table->basic->skiplist,
                             PPDB_SKIPLIST_MAX_LEVEL,
                             ppdb_skiplist_default_compare,
                             &sync_config);
    if (err != PPDB_OK) {
        ppdb_sync_destroy(&new_table->basic->sync);
        free(new_table->basic);
        free(new_table);
        return err;
    }

    *table = new_table;
    return PPDB_OK;
}

// 销毁内存表
void ppdb_memtable_destroy_basic(ppdb_memtable_t* table) {
    if (!table) return;
    
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
    if (!table || !table->basic || !key || !value) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0 || value_len == 0) return PPDB_ERR_INVALID_ARG;
    if (table->is_immutable) return PPDB_ERR_MEMTABLE_IMMUTABLE;

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
    if (!table || !table->basic || !key || !value || !value_len) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARG;

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
    if (!table || !table->basic || !key) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARG;
    if (table->is_immutable) return PPDB_ERR_MEMTABLE_IMMUTABLE;

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
