#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_sharded_memtable.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/skiplist.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/metrics.h"

// 分片内存表结构
struct ppdb_sharded_memtable {
    size_t shard_count;
    ppdb_memtable_t** shards;
};

// 分片迭代器内部结构
typedef struct {
    ppdb_sharded_memtable_t* table;
    size_t current_shard;
    ppdb_memtable_iterator_t** shard_iterators;  // 每个分片的迭代器
    ppdb_kv_pair_t** current_pairs;              // 每个分片的当前键值对
    bool valid;
} sharded_iterator_internal_t;

// 比较两个键值对
static int compare_pairs(const ppdb_kv_pair_t* a, const ppdb_kv_pair_t* b) {
    size_t min_len = a->key_size < b->key_size ? a->key_size : b->key_size;
    int cmp = memcmp(a->key, b->key, min_len);
    if (cmp != 0) return cmp;
    return (a->key_size < b->key_size) ? -1 : (a->key_size > b->key_size ? 1 : 0);
}

// 找到具有最小键的分片索引
static size_t find_min_key_shard(sharded_iterator_internal_t* internal) {
    size_t min_shard = (size_t)-1;
    
    for (size_t i = 0; i < internal->table->shard_count; i++) {
        if (internal->current_pairs[i] && (min_shard == (size_t)-1 || 
            compare_pairs(internal->current_pairs[i], internal->current_pairs[min_shard]) < 0)) {
            min_shard = i;
        }
    }
    
    return min_shard;
}

// 迭代器前进
static bool sharded_iterator_next(ppdb_iterator_t* iter) {
    if (!iter || !iter->internal) {
        return false;
    }

    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    if (!internal->valid) {
        return false;
    }

    // 如果是第一次调用，初始化所有分片的迭代器
    if (!internal->shard_iterators) {
        internal->shard_iterators = calloc(internal->table->shard_count, sizeof(ppdb_memtable_iterator_t*));
        internal->current_pairs = calloc(internal->table->shard_count, sizeof(ppdb_kv_pair_t*));
        if (!internal->shard_iterators || !internal->current_pairs) {
            internal->valid = false;
            return false;
        }

        // 初始化每个分片的迭代器
        for (size_t i = 0; i < internal->table->shard_count; i++) {
            ppdb_error_t err = ppdb_memtable_iterator_create_basic(
                internal->table->shards[i],
                &internal->shard_iterators[i]);
            
            if (err == PPDB_OK && internal->shard_iterators[i]) {
                ppdb_kv_pair_t* next_pair = NULL;
                err = ppdb_memtable_iterator_next_basic(internal->shard_iterators[i], &next_pair);
                if (err == PPDB_OK && next_pair) {
                    internal->current_pairs[i] = malloc(sizeof(ppdb_kv_pair_t));
                    if (internal->current_pairs[i]) {
                        // 复制键值对
                        internal->current_pairs[i]->key = malloc(next_pair->key_size);
                        internal->current_pairs[i]->value = malloc(next_pair->value_size);
                        if (internal->current_pairs[i]->key && internal->current_pairs[i]->value) {
                            memcpy(internal->current_pairs[i]->key, next_pair->key, next_pair->key_size);
                            memcpy(internal->current_pairs[i]->value, next_pair->value, next_pair->value_size);
                            internal->current_pairs[i]->key_size = next_pair->key_size;
                            internal->current_pairs[i]->value_size = next_pair->value_size;
                        } else {
                            if (internal->current_pairs[i]->key) free(internal->current_pairs[i]->key);
                            if (internal->current_pairs[i]->value) free(internal->current_pairs[i]->value);
                            free(internal->current_pairs[i]);
                            internal->current_pairs[i] = NULL;
                        }
                    }
                }
            }
        }
    } else {
        // 找到当前最小键的分片
        size_t min_shard = find_min_key_shard(internal);
        if (min_shard == (size_t)-1) {
            internal->valid = false;
            return false;
        }

        // 获取该分片的下一个键值对
        ppdb_kv_pair_t* next_pair = NULL;
        ppdb_error_t err = ppdb_memtable_iterator_next_basic(internal->shard_iterators[min_shard], &next_pair);
        if (err == PPDB_OK && next_pair) {
            // 释放当前最小键的内存
            if (internal->current_pairs[min_shard]) {
                free(internal->current_pairs[min_shard]->key);
                free(internal->current_pairs[min_shard]->value);
                free(internal->current_pairs[min_shard]);
            }

            // 更新为新的键值对
            internal->current_pairs[min_shard] = malloc(sizeof(ppdb_kv_pair_t));
            if (internal->current_pairs[min_shard]) {
                // 复制键值对
                internal->current_pairs[min_shard]->key = malloc(next_pair->key_size);
                internal->current_pairs[min_shard]->value = malloc(next_pair->value_size);
                if (internal->current_pairs[min_shard]->key && internal->current_pairs[min_shard]->value) {
                    memcpy(internal->current_pairs[min_shard]->key, next_pair->key, next_pair->key_size);
                    memcpy(internal->current_pairs[min_shard]->value, next_pair->value, next_pair->value_size);
                    internal->current_pairs[min_shard]->key_size = next_pair->key_size;
                    internal->current_pairs[min_shard]->value_size = next_pair->value_size;
                } else {
                    if (internal->current_pairs[min_shard]->key) free(internal->current_pairs[min_shard]->key);
                    if (internal->current_pairs[min_shard]->value) free(internal->current_pairs[min_shard]->value);
                    free(internal->current_pairs[min_shard]);
                    internal->current_pairs[min_shard] = NULL;
                }
            }
        } else {
            // 如果没有下一个键值对，释放当前的并设置为 NULL
            if (internal->current_pairs[min_shard]) {
                free(internal->current_pairs[min_shard]->key);
                free(internal->current_pairs[min_shard]->value);
                free(internal->current_pairs[min_shard]);
                internal->current_pairs[min_shard] = NULL;
            }
        }
    }

    // 检查是否还有有效的键值对
    internal->valid = find_min_key_shard(internal) != (size_t)-1;
    return internal->valid;
}

// 获取当前键值对
static ppdb_error_t sharded_iterator_get(ppdb_iterator_t* iter, ppdb_kv_pair_t* pair) {
    if (!iter || !iter->internal || !pair) {
        return PPDB_ERR_NULL_POINTER;
    }

    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    if (!internal->valid) {
        return PPDB_ERR_INVALID_STATE;
    }

    size_t min_shard = find_min_key_shard(internal);
    if (min_shard == (size_t)-1) {
        return PPDB_ERR_NOT_FOUND;
    }

    ppdb_kv_pair_t* min_pair = internal->current_pairs[min_shard];
    
    // 复制键
    pair->key = malloc(min_pair->key_size);
    if (!pair->key) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    memcpy(pair->key, min_pair->key, min_pair->key_size);
    pair->key_size = min_pair->key_size;

    // 复制值
    pair->value = malloc(min_pair->value_size);
    if (!pair->value) {
        free(pair->key);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    memcpy(pair->value, min_pair->value, min_pair->value_size);
    pair->value_size = min_pair->value_size;

    return PPDB_OK;
}

// 检查迭代器是否有效
static bool sharded_iterator_valid(const ppdb_iterator_t* iter) {
    if (!iter || !iter->internal) {
        return false;
    }

    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    return internal->valid;
}

// 销毁迭代器
void ppdb_iterator_destroy(ppdb_iterator_t* iter) {
    if (!iter) {
        return;
    }

    sharded_iterator_internal_t* internal = (sharded_iterator_internal_t*)iter->internal;
    if (internal) {
        if (internal->shard_iterators) {
            for (size_t i = 0; i < internal->table->shard_count; i++) {
                if (internal->shard_iterators[i]) {
                    ppdb_memtable_iterator_destroy_basic(internal->shard_iterators[i]);
                }
            }
            free(internal->shard_iterators);
        }
        if (internal->current_pairs) {
            for (size_t i = 0; i < internal->table->shard_count; i++) {
                if (internal->current_pairs[i]) {
                    free(internal->current_pairs[i]->key);
                    free(internal->current_pairs[i]->value);
                    free(internal->current_pairs[i]);
                }
            }
            free(internal->current_pairs);
        }
        free(internal);
    }
    free(iter);
}

// 创建分片迭代器
ppdb_error_t ppdb_sharded_memtable_iterator_create(ppdb_sharded_memtable_t* table, ppdb_iterator_t** iter) {
    if (!table || !iter) {
        return PPDB_ERR_NULL_POINTER;
    }

    // 创建迭代器结构
    ppdb_iterator_t* new_iter = malloc(sizeof(ppdb_iterator_t));
    if (!new_iter) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 创建内部结构
    sharded_iterator_internal_t* internal = malloc(sizeof(sharded_iterator_internal_t));
    if (!internal) {
        free(new_iter);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    internal->table = table;
    internal->current_shard = 0;
    internal->shard_iterators = NULL;
    internal->current_pairs = NULL;
    internal->valid = true;

    new_iter->internal = internal;
    new_iter->next = sharded_iterator_next;
    new_iter->valid = sharded_iterator_valid;
    new_iter->get = sharded_iterator_get;

    // 移动到第一个元素
    *iter = new_iter;
    if (!sharded_iterator_next(new_iter)) {
        ppdb_iterator_destroy(new_iter);
        *iter = NULL;
        return PPDB_ERR_NOT_FOUND;
    }

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

// MurmurHash3 算法的辅助函数
static uint64_t rotl64(uint64_t x, int8_t r) {
    return (x << r) | (x >> (64 - r));
}

static uint64_t fmix64(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

// 获取分片索引
size_t ppdb_sharded_memtable_get_shard_index(ppdb_sharded_memtable_t* table, const void* key, size_t key_len) {
    if (!table || !key || key_len == 0 || table->shard_count == 0) return 0;

    const uint64_t c1 = 0x87c37b91114253d5ULL;
    const uint64_t c2 = 0x4cf5ad432745937fULL;
    const unsigned char* data = (const unsigned char*)key;
    const int nblocks = key_len / 16;
    uint64_t h1 = 0;
    uint64_t h2 = 0;

    // 处理16字节的块
    const uint64_t* blocks = (const uint64_t*)(data);
    for (int i = 0; i < nblocks; i++) {
        uint64_t k1 = blocks[i*2];
        uint64_t k2 = blocks[i*2+1];

        k1 *= c1; k1 = rotl64(k1,31); k1 *= c2; h1 ^= k1;
        h1 = rotl64(h1,27); h1 += h2; h1 = h1*5+0x52dce729;

        k2 *= c2; k2 = rotl64(k2,33); k2 *= c1; h2 ^= k2;
        h2 = rotl64(h2,31); h2 += h1; h2 = h2*5+0x38495ab5;
    }

    // 处理剩余字节
    const unsigned char* tail = data + nblocks*16;
    uint64_t k1 = 0;
    uint64_t k2 = 0;
    switch(key_len & 15) {
        case 15: k2 ^= ((uint64_t)tail[14]) << 48;
                __attribute__((fallthrough));
        case 14: k2 ^= ((uint64_t)tail[13]) << 40;
                __attribute__((fallthrough));
        case 13: k2 ^= ((uint64_t)tail[12]) << 32;
                __attribute__((fallthrough));
        case 12: k2 ^= ((uint64_t)tail[11]) << 24;
                __attribute__((fallthrough));
        case 11: k2 ^= ((uint64_t)tail[10]) << 16;
                __attribute__((fallthrough));
        case 10: k2 ^= ((uint64_t)tail[ 9]) << 8;
                __attribute__((fallthrough));
        case  9: k2 ^= ((uint64_t)tail[ 8]) << 0;
                k2 *= c2; k2 = rotl64(k2,33); k2 *= c1; h2 ^= k2;
                __attribute__((fallthrough));

        case  8: k1 ^= ((uint64_t)tail[ 7]) << 56;
                __attribute__((fallthrough));
        case  7: k1 ^= ((uint64_t)tail[ 6]) << 48;
                __attribute__((fallthrough));
        case  6: k1 ^= ((uint64_t)tail[ 5]) << 40;
                __attribute__((fallthrough));
        case  5: k1 ^= ((uint64_t)tail[ 4]) << 32;
                __attribute__((fallthrough));
        case  4: k1 ^= ((uint64_t)tail[ 3]) << 24;
                __attribute__((fallthrough));
        case  3: k1 ^= ((uint64_t)tail[ 2]) << 16;
                __attribute__((fallthrough));
        case  2: k1 ^= ((uint64_t)tail[ 1]) << 8;
                __attribute__((fallthrough));
        case  1: k1 ^= ((uint64_t)tail[ 0]) << 0;
                k1 *= c1; k1 = rotl64(k1,31); k1 *= c2; h1 ^= k1;
    };

    h1 ^= key_len;
    h2 ^= key_len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;
    h2 += h1;

    return h1 % table->shard_count;
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
