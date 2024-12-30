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

// 创建内存表
ppdb_error_t ppdb_memtable_create_basic(size_t size, ppdb_memtable_t** table) {
    if (!table) return PPDB_ERR_INVALID_ARG;
    
    ppdb_memtable_t* new_table = aligned_alloc(64, sizeof(ppdb_memtable_t));
    if (!new_table) return PPDB_ERR_NO_MEMORY;

    new_table->skiplist = NULL;
    new_table->used = 0;
    new_table->size = size;
    new_table->is_immutable = false;
    memset(&new_table->metrics, 0, sizeof(ppdb_metrics_t));

    ppdb_error_t err = ppdb_skiplist_create((ppdb_skiplist_t**)&new_table->skiplist);
    if (err != PPDB_OK) {
        free(new_table);
        return err;
    }

    ppdb_sync_config_t sync_config = {
        .use_lockfree = false,
        .stripe_count = 0,
        .spin_count = 1000,
        .yield_count = 100,
        .sleep_time = 1000,
        .backoff_us = 100,
        .enable_ref_count = false
    };
    err = ppdb_sync_init(&new_table->sync, &sync_config);
    if (err != PPDB_OK) {
        ppdb_skiplist_destroy(new_table->skiplist);
        free(new_table);
        return err;
    }

    *table = new_table;
    return PPDB_OK;
}

// 销毁内存表
void ppdb_memtable_destroy_basic(ppdb_memtable_t* table) {
    if (!table) return;
    
    ppdb_skiplist_destroy(table->skiplist);
    ppdb_sync_destroy(&table->sync);
    free(table);
}

// 写入键值对
ppdb_error_t ppdb_memtable_put_basic(ppdb_memtable_t* table,
                                    const void* key, size_t key_len,
                                    const void* value, size_t value_len) {
    if (!table || !key || !value) return PPDB_ERR_INVALID_ARG;
    if (table->is_immutable) return PPDB_ERR_READONLY;
    
    size_t total_size = key_len + value_len + PPDB_SKIPLIST_NODE_SIZE;
    if (table->used + total_size > table->size) {
        return PPDB_ERR_NO_SPACE;
    }

    ppdb_sync_lock(&table->sync);

    ppdb_error_t err = ppdb_skiplist_put(table->skiplist, 
                                        (const uint8_t*)key, key_len,
                                        (const uint8_t*)value, value_len);
    if (err == PPDB_OK) {
        table->used += total_size;
        atomic_fetch_add(&table->metrics.put_count, 1);
    }

    ppdb_sync_unlock(&table->sync);
    return err;
}

// 读取键值对
ppdb_error_t ppdb_memtable_get_basic(ppdb_memtable_t* table,
                                    const void* key, size_t key_len,
                                    void* value, size_t* value_len) {
    if (!table || !key || !value_len) return PPDB_ERR_INVALID_ARG;

    ppdb_sync_lock(&table->sync);

    uint8_t* tmp_value;
    ppdb_error_t err = ppdb_skiplist_get(table->skiplist,
                                        (const uint8_t*)key, key_len,
                                        &tmp_value, value_len);
    if (err == PPDB_OK) {
        memcpy(value, tmp_value, *value_len);
        atomic_fetch_add(&table->metrics.get_count, 1);
    }

    ppdb_sync_unlock(&table->sync);
    return err;
}

// 删除键值对
ppdb_error_t ppdb_memtable_delete_basic(ppdb_memtable_t* table,
                                       const void* key, size_t key_len) {
    if (!table || !key) return PPDB_ERR_INVALID_ARG;
    if (table->is_immutable) return PPDB_ERR_READONLY;

    ppdb_sync_lock(&table->sync);

    ppdb_error_t err = ppdb_skiplist_delete(table->skiplist,
                                           (const uint8_t*)key, key_len);
    if (err == PPDB_OK) {
        table->used -= (key_len + PPDB_SKIPLIST_NODE_SIZE);
        atomic_fetch_add(&table->metrics.delete_count, 1);
    }

    ppdb_sync_unlock(&table->sync);
    return err;
}

// 获取当前大小
size_t ppdb_memtable_size_basic(ppdb_memtable_t* table) {
    return table ? table->used : 0;
}

// 获取最大大小
size_t ppdb_memtable_max_size_basic(ppdb_memtable_t* table) {
    return table ? table->size : 0;
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
