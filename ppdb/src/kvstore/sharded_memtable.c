#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"

// 内部头文件
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_logger.h"
#include "kvstore/internal/kvstore_fs.h"
#include "kvstore/internal/skiplist.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/metrics.h"
#include "kvstore/internal/kvstore_sharded_memtable.h"

// 分片内存表结构
struct ppdb_sharded_memtable {
    size_t shard_count;
    ppdb_memtable_t** shards;
};

// 分片迭代器内部结构
typedef struct {
    ppdb_sharded_memtable_t* table;
    size_t current_shard;
    ppdb_memtable_iterator_t* current_iterator;
} sharded_iterator_internal_t;

// 迭代器前进
static bool sharded_iterator_next(ppdb_iterator_t* iter) {
    if (!iter || !iter->internal) return false;
    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    
    if (!internal->current_iterator) return false;
    
    // 尝试在当前分片中前进
    ppdb_kv_pair_t* pair;
    ppdb_error_t err = ppdb_memtable_iterator_next_basic(internal->current_iterator, &pair);
    
    // 如果当前分片迭代完成，移动到下一个分片
    if (err != PPDB_OK) {
        ppdb_memtable_iterator_destroy_basic(internal->current_iterator);
        internal->current_iterator = NULL;
        
        // 寻找下一个非空分片
        while (internal->current_shard < internal->table->shard_count - 1) {
            internal->current_shard++;
            
            err = ppdb_memtable_iterator_create_basic(
                internal->table->shards[internal->current_shard],
                &internal->current_iterator);
            
            if (err != PPDB_OK) {
                internal->current_iterator = NULL;
                return false;
            }
            
            // 检查新分片是否有数据
            ppdb_kv_pair_t pair;
            err = ppdb_memtable_iterator_get_basic(internal->current_iterator, &pair);
            if (err == PPDB_OK) {
                return true;
            }
            
            ppdb_memtable_iterator_destroy_basic(internal->current_iterator);
            internal->current_iterator = NULL;
        }
        return false;
    }
    
    return true;
}

// 检查迭代器是否有效
static bool sharded_iterator_valid(const ppdb_iterator_t* iter) {
    if (!iter || !iter->internal) return false;
    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    return internal->current_iterator != NULL;
}

// 获取当前键值对
static ppdb_error_t sharded_iterator_get(ppdb_iterator_t* iter, ppdb_kv_pair_t* pair) {
    if (!iter || !iter->internal || !pair) return PPDB_ERR_INVALID_ARG;
    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    if (!internal->current_iterator) return PPDB_ERR_INVALID_ARG;
    
    ppdb_error_t err = ppdb_memtable_iterator_get_basic(internal->current_iterator, pair);
    if (err != PPDB_OK) return err;
    
    return PPDB_OK;
}

// 创建分片内存表
ppdb_error_t ppdb_sharded_memtable_create(ppdb_sharded_memtable_t** table, size_t shard_count) {
    if (!table || shard_count == 0) return PPDB_ERR_INVALID_ARG;

    ppdb_sharded_memtable_t* new_table = malloc(sizeof(ppdb_sharded_memtable_t));
    if (!new_table) return PPDB_ERR_OUT_OF_MEMORY;

    new_table->shard_count = shard_count;
    new_table->shards = calloc(shard_count, sizeof(ppdb_memtable_t*));
    if (!new_table->shards) {
        free(new_table);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < shard_count; i++) {
        ppdb_error_t err = ppdb_memtable_create_sharded_basic(0, &new_table->shards[i]);
        if (err != PPDB_OK) {
            for (size_t j = 0; j < i; j++) {
                ppdb_memtable_destroy_sharded(new_table->shards[j]);
            }
            free(new_table->shards);
            free(new_table);
            return err;
        }
    }

    *table = new_table;
    return PPDB_OK;
}

// 销毁分片内存表
void ppdb_sharded_memtable_destroy(ppdb_sharded_memtable_t* table) {
    if (!table) return;
    if (table->shards) {
        for (size_t i = 0; i < table->shard_count; i++) {
            ppdb_memtable_destroy_sharded(table->shards[i]);
        }
        free(table->shards);
    }
    free(table);
}

// 获取分片索引
size_t ppdb_sharded_memtable_get_shard_index(ppdb_sharded_memtable_t* table, const void* key, size_t key_len) {
    if (!table || !key) return 0;

    // 按照键的第一个字符分片，这样可以保证相近的键在同一个分片中
    const uint8_t* bytes = (const uint8_t*)key;
    return bytes[0] % table->shard_count;
}

// 写入键值对
ppdb_error_t ppdb_sharded_memtable_put(ppdb_sharded_memtable_t* table, const void* key, size_t key_len, const void* value, size_t value_len) {
    if (!table || !key || !value) return PPDB_ERR_INVALID_ARG;
    size_t shard = ppdb_sharded_memtable_get_shard_index(table, key, key_len);
    return ppdb_memtable_put_sharded_basic(table->shards[shard], key, key_len, value, value_len);
}

// 读取键值对
ppdb_error_t ppdb_sharded_memtable_get(ppdb_sharded_memtable_t* table, const void* key, size_t key_len, void* value, size_t* value_len) {
    if (!table || !key || !value || !value_len) return PPDB_ERR_INVALID_ARG;
    size_t shard = ppdb_sharded_memtable_get_shard_index(table, key, key_len);
    return ppdb_memtable_get_sharded_basic(table->shards[shard], key, key_len, value, value_len);
}

// 删除键值对
ppdb_error_t ppdb_sharded_memtable_delete(ppdb_sharded_memtable_t* table, const void* key, size_t key_len) {
    if (!table || !key) return PPDB_ERR_INVALID_ARG;
    size_t shard = ppdb_sharded_memtable_get_shard_index(table, key, key_len);
    return ppdb_memtable_delete_sharded_basic(table->shards[shard], key, key_len);
}

// 创建迭代器
ppdb_error_t ppdb_sharded_memtable_iterator_create(ppdb_sharded_memtable_t* table, ppdb_iterator_t** iter) {
    if (!table || !iter) return PPDB_ERR_INVALID_ARG;

    ppdb_iterator_t* new_iter = malloc(sizeof(ppdb_iterator_t));
    if (!new_iter) return PPDB_ERR_OUT_OF_MEMORY;

    sharded_iterator_internal_t* internal = malloc(sizeof(sharded_iterator_internal_t));
    if (!internal) {
        free(new_iter);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    internal->table = table;
    internal->current_shard = 0;
    internal->current_iterator = NULL;

    // 找到第一个非空的分片
    while (internal->current_shard < table->shard_count) {
        ppdb_error_t err = ppdb_memtable_iterator_create_basic(
            table->shards[internal->current_shard],
            &internal->current_iterator);
        
        if (err != PPDB_OK) {
            free(internal);
            free(new_iter);
            return err;
        }

        ppdb_kv_pair_t pair;
        err = ppdb_memtable_iterator_get_basic(internal->current_iterator, &pair);
        if (err == PPDB_OK) {
            break;
        }
        
        ppdb_memtable_iterator_destroy_basic(internal->current_iterator);
        internal->current_iterator = NULL;
        internal->current_shard++;
    }

    new_iter->internal = internal;
    new_iter->next = sharded_iterator_next;
    new_iter->valid = sharded_iterator_valid;
    new_iter->get = sharded_iterator_get;

    *iter = new_iter;
    return PPDB_OK;
}

// 销毁迭代器
void ppdb_iterator_destroy(ppdb_iterator_t* iter) {
    if (!iter || !iter->internal) return;
    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    if (internal->current_iterator) {
        ppdb_memtable_iterator_destroy_basic(internal->current_iterator);
    }
    free(internal);
    free(iter);
}
