#ifndef PPDB_KVSTORE_TYPES_H
#define PPDB_KVSTORE_TYPES_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"

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