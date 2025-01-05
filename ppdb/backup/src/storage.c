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
            
            // 销毁头节点
            if (shard->head) {
                node_unref(shard->head);
                shard->head = NULL;
            }
            
            // 释放并销毁分片锁
            if (shard->lock) {
                ppdb_sync_destroy(shard->lock);
                PPDB_ALIGNED_FREE(shard->lock);
                shard->lock = NULL;
            }
        }
        PPDB_ALIGNED_FREE(base->shards);
        base->shards = NULL;
    }
    
    PPDB_ALIGNED_FREE(base);
}

//
#include "storage_crud.inc.c"
//
#include "storage_iterator.inc.c"
//
#include "storage_misc.inc.c"

void init_random(void) {
    srand((unsigned int)time(NULL));
}

ppdb_error_t init_metrics(ppdb_metrics_t* metrics) {
    if (!metrics) return PPDB_ERR_NULL_POINTER;
    
    ppdb_error_t err;
    
    err = ppdb_sync_counter_init(&metrics->ops_count, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->bytes_written, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->bytes_read, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->total_nodes, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->total_keys, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->total_bytes, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->total_gets, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->total_puts, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->total_removes, 0);
    if (err != PPDB_OK) return err;
    
    return PPDB_OK;
}

uint32_t random_level(void) {
    uint32_t level = 1;
    while (level < PPDB_MAX_LEVEL && ((double)rand() / RAND_MAX) < PPDB_LEVEL_PROBABILITY) {
        level++;
    }
    return level;
}

#endif // PPDB_STORAGE_C