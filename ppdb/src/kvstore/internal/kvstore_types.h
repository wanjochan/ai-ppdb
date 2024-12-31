#ifndef PPDB_KVSTORE_TYPES_H
#define PPDB_KVSTORE_TYPES_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"

// 迭代器结构
typedef struct ppdb_iterator {
    void* internal;
    bool (*next)(struct ppdb_iterator*);
    bool (*valid)(const struct ppdb_iterator*);
    ppdb_error_t (*get)(struct ppdb_iterator*, ppdb_kv_pair_t*);
} ppdb_iterator_t;

#endif // PPDB_KVSTORE_TYPES_H