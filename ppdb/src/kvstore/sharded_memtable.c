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
    if (!iter) {
        return false;
    }

    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    if (!internal) {
        return false;
    }

    // 如果当前迭代器为空，尝试获取下一个分片的迭代器
    if (!internal->current_iterator) {
        while (internal->current_shard < internal->table->shard_count) {
            ppdb_error_t err = ppdb_memtable_iterator_create_basic(
                internal->table->shards[internal->current_shard],
                &internal->current_iterator);
            
            if (err == PPDB_OK && internal->current_iterator) {
                ppdb_kv_pair_t* first_pair = NULL;
                err = ppdb_memtable_iterator_next_basic(internal->current_iterator, &first_pair);
                if (err == PPDB_OK && first_pair) {
                    free(first_pair);
                    return true;  // 找到有效的迭代器和第一个元素
                }
            }
            // 如果获取迭代器失败或没有第一个元素，继续查找下一个分片
            if (internal->current_iterator) {
                ppdb_memtable_iterator_destroy_basic(internal->current_iterator);
                internal->current_iterator = NULL;
            }
            internal->current_shard++;
        }
        return false;  // 没有更多的分片
    }

    // 尝试移动到当前迭代器的下一个键值对
    ppdb_kv_pair_t* pair = NULL;
    ppdb_error_t err = ppdb_memtable_iterator_next_basic(internal->current_iterator, &pair);
    if (err == PPDB_OK) {
        if (pair) {
            free(pair);
        }
        return true;  // 成功移动到下一个键值对
    }

    // 如果当前分片已遍历完，释放当前迭代器并尝试下一个分片
    ppdb_memtable_iterator_destroy_basic(internal->current_iterator);
    internal->current_iterator = NULL;
    internal->current_shard++;
    return sharded_iterator_next(iter);
}

// 获取当前键值对
static ppdb_error_t sharded_iterator_get(ppdb_iterator_t* iter, ppdb_kv_pair_t* pair_out) {
    if (!iter || !pair_out) {
        return PPDB_ERR_INVALID_ARG;
    }

    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    if (!internal || !internal->current_iterator) {
        return PPDB_ERR_NOT_FOUND;
    }

    return ppdb_memtable_iterator_get_basic(internal->current_iterator, pair_out);
}

// 检查迭代器是否有效
static bool sharded_iterator_valid(const ppdb_iterator_t* iter) {
    if (!iter) {
        return false;
    }

    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    if (!internal) {
        return false;
    }

    // 如果当前迭代器为空，尝试获取下一个分片的迭代器
    if (!internal->current_iterator) {
        while (internal->current_shard < internal->table->shard_count) {
            ppdb_error_t err = ppdb_memtable_iterator_create_basic(
                internal->table->shards[internal->current_shard],
                &internal->current_iterator);
            
            if (err == PPDB_OK && internal->current_iterator) {
                ppdb_kv_pair_t* pair = NULL;
                if (ppdb_memtable_iterator_next_basic(internal->current_iterator, &pair) == PPDB_OK) {
                    if (pair) {
                        free(pair->key);
                        free(pair->value);
                        free(pair);
                    }
                    return true;  // 找到有效的迭代器
                }
                // 如果获取键值对失败，释放当前迭代器并继续查找
                ppdb_memtable_iterator_destroy_basic(internal->current_iterator);
                internal->current_iterator = NULL;
            }
            internal->current_shard++;
        }
        return false;  // 没有更多的分片
    }

    // 检查当前迭代器是否有效
    ppdb_kv_pair_t* pair = NULL;
    ppdb_error_t err = ppdb_memtable_iterator_next_basic(internal->current_iterator, &pair);
    if (err != PPDB_OK) {
        // 如果当前迭代器无效，释放当前迭代器并尝试下一个分片
        ppdb_memtable_iterator_destroy_basic(internal->current_iterator);
        internal->current_iterator = NULL;
        internal->current_shard++;
        return sharded_iterator_valid(iter);
    }
    if (pair) {
        free(pair->key);
        free(pair->value);
        free(pair);
    }

    return true;
}

// 销毁迭代器
void ppdb_iterator_destroy(ppdb_iterator_t* iter) {
    if (!iter) {
        return;
    }

    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    if (internal) {
        if (internal->current_iterator) {
            ppdb_memtable_iterator_destroy_basic(internal->current_iterator);
        }
        free(internal);
    }
    free(iter);
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

    // 为每个分片创建内存表
    size_t shard_size = 4096 * 1024;  // 每个分片4MB
    for (size_t i = 0; i < shard_count; i++) {
        ppdb_error_t err = ppdb_memtable_create_basic(shard_size, &new_table->shards[i]);
        if (err != PPDB_OK) {
            // 清理已创建的分片
            for (size_t j = 0; j < i; j++) {
                ppdb_memtable_destroy_basic(new_table->shards[j]);
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
            ppdb_memtable_destroy_basic(table->shards[i]);
        }
        free(table->shards);
    }
    free(table);
}

// 获取分片索引
size_t ppdb_sharded_memtable_get_shard_index(ppdb_sharded_memtable_t* table, const void* key, size_t key_len) {
    if (!table || !key || key_len == 0 || table->shard_count == 0) return 0;

    // 使用简单的加法哈希
    const unsigned char* data = (const unsigned char*)key;
    uint64_t hash = 0;
    
    for (size_t i = 0; i < key_len; i++) {
        hash = hash * 31 + data[i];
    }

    return hash % table->shard_count;
}

// 写入键值对
ppdb_error_t ppdb_sharded_memtable_put(ppdb_sharded_memtable_t* table, const void* key, size_t key_len, const void* value, size_t value_len) {
    if (!table || !key || !value || key_len == 0 || value_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 获取分片索引
    size_t shard_index = ppdb_sharded_memtable_get_shard_index(table, key, key_len);
    if (shard_index >= table->shard_count) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 复制键值对
    void* key_copy = malloc(key_len);
    if (!key_copy) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    void* value_copy = malloc(value_len);
    if (!value_copy) {
        free(key_copy);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    memcpy(key_copy, key, key_len);
    memcpy(value_copy, value, value_len);

    // 写入分片
    ppdb_error_t err = ppdb_memtable_put_basic(table->shards[shard_index], key_copy, key_len, value_copy, value_len);
    if (err != PPDB_OK) {
        free(key_copy);
        free(value_copy);
        return err;
    }

    return PPDB_OK;
}

// 读取键值对
ppdb_error_t ppdb_sharded_memtable_get(ppdb_sharded_memtable_t* table, const void* key, size_t key_len, void** value_out, size_t* value_len_out) {
    if (!table || !key || !value_out || !value_len_out || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 获取分片索引
    size_t shard_index = ppdb_sharded_memtable_get_shard_index(table, key, key_len);
    if (shard_index >= table->shard_count || !table->shards[shard_index]) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 从分片中读取
    return ppdb_memtable_get_basic(table->shards[shard_index], key, key_len, value_out, value_len_out);
}

// 删除键值对
ppdb_error_t ppdb_sharded_memtable_delete(ppdb_sharded_memtable_t* table, const void* key, size_t key_len) {
    if (!table || !key || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 获取分片索引
    size_t shard_index = ppdb_sharded_memtable_get_shard_index(table, key, key_len);
    if (shard_index >= table->shard_count) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 从分片中删除
    return ppdb_memtable_delete_basic(table->shards[shard_index], key, key_len);
}

// 创建分片迭代器
ppdb_error_t ppdb_sharded_memtable_iterator_create(ppdb_sharded_memtable_t* table,
                                                  ppdb_iterator_t** iter_out) {
    if (!table || !iter_out) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 分配迭代器内存
    ppdb_iterator_t* iter = malloc(sizeof(ppdb_iterator_t));
    if (!iter) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 分配内部数据结构内存
    sharded_iterator_internal_t* internal = malloc(sizeof(sharded_iterator_internal_t));
    if (!internal) {
        free(iter);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化内部数据
    internal->table = table;
    internal->current_shard = 0;
    internal->current_iterator = NULL;

    // 初始化第一个有效的分片迭代器
    while (internal->current_shard < table->shard_count) {
        ppdb_error_t err = ppdb_memtable_iterator_create_basic(
            table->shards[internal->current_shard],
            &internal->current_iterator);
        
        if (err == PPDB_OK && internal->current_iterator) {
            ppdb_kv_pair_t* pair = NULL;
            if (ppdb_memtable_iterator_next_basic(internal->current_iterator, &pair) == PPDB_OK) {
                if (pair) {
                    free(pair->key);
                    free(pair->value);
                    free(pair);
                }
                break;  // 找到有效的迭代器
            }
            // 如果获取键值对失败，释放当前迭代器并继续查找
            ppdb_memtable_iterator_destroy_basic(internal->current_iterator);
            internal->current_iterator = NULL;
        }
        internal->current_shard++;
    }

    // 设置迭代器函数
    iter->next = sharded_iterator_next;
    iter->get = sharded_iterator_get;
    iter->valid = sharded_iterator_valid;
    iter->internal = internal;

    *iter_out = iter;
    return PPDB_OK;
}
