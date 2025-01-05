#include "ppdb/ppdb.h"
#include "ppdb/internal.h"
#include <cosmopolitan.h>

#ifndef PPDB_STORAGE_C
#define PPDB_STORAGE_C

// 键值操作实现
ppdb_error_t ppdb_key_copy(ppdb_key_t* dst, const ppdb_key_t* src) {
    if (!dst || !src) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    dst->size = src->size;
    dst->data = PPDB_ALIGNED_ALLOC(dst->size);
    if (!dst->data) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    memcpy(dst->data, src->data, dst->size);
    return PPDB_OK;
}

void ppdb_key_cleanup(ppdb_key_t* key) {
    if (!key) {
        return;
    }
    if (key->data) {
        PPDB_ALIGNED_FREE(key->data);
        key->data = NULL;
    }
    key->size = 0;
}

ppdb_error_t ppdb_value_copy(ppdb_value_t* dst, const ppdb_value_t* src) {
    if (!dst || !src) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    dst->size = src->size;
    dst->data = PPDB_ALIGNED_ALLOC(dst->size);
    if (!dst->data) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    memcpy(dst->data, src->data, dst->size);
    ppdb_sync_counter_init(&dst->ref_count, 1);
    return PPDB_OK;
}

void ppdb_value_cleanup(ppdb_value_t* value) {
    if (!value) {
        return;
    }
    if (value->data) {
        PPDB_ALIGNED_FREE(value->data);
        value->data = NULL;
    }
    value->size = 0;
}

void cleanup_base(ppdb_base_t* base) {
    if (!base) {
        return;
    }

    if (base->shards) {
        for (uint32_t i = 0; i < base->config.shard_count; i++) {
            ppdb_shard_t* shard = &base->shards[i];
            if (shard->head) {
                node_unref(shard->head);
            }
            if (shard->lock) {
                ppdb_sync_destroy(shard->lock);
                PPDB_ALIGNED_FREE(shard->lock);
            }
        }
        PPDB_ALIGNED_FREE(base->shards);
    }
}

//
#include "storage_crud.inc.c"
//
#include "storage_iterator.inc.c"
//
#include "storage_misc.inc.c"

#endif // PPDB_STORAGE_C