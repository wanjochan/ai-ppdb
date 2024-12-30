#ifndef PPDB_KVSTORE_TYPES_H
#define PPDB_KVSTORE_TYPES_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"

// 从 defs.h 和 types.h 合并的定义
#define PPDB_MAX_KEY_SIZE 1024
#define PPDB_MAX_VALUE_SIZE (1024 * 1024)
#define PPDB_MAX_PATH_SIZE 256
#define PPDB_DEFAULT_MEMTABLE_SIZE (64 * 1024 * 1024)  // 64MB
#define PPDB_DEFAULT_WAL_SEGMENT_SIZE (4 * 1024 * 1024)  // 4MB

// 基础类型定义
typedef uint32_t ppdb_size_t;
typedef uint64_t ppdb_offset_t;
typedef uint64_t ppdb_timestamp_t;
typedef uint32_t ppdb_version_t;

// 键值对结构
typedef struct ppdb_kv_pair {
    void* key;
    void* value;
    ppdb_size_t key_len;
    ppdb_size_t value_len;
} ppdb_kv_pair_t;

// 迭代器结构
typedef struct ppdb_iterator {
    void* internal;
    bool (*next)(struct ppdb_iterator*);
    bool (*valid)(const struct ppdb_iterator*);
    ppdb_error_t (*get)(struct ppdb_iterator*, ppdb_kv_pair_t*);
} ppdb_iterator_t;

#endif // PPDB_KVSTORE_TYPES_H 