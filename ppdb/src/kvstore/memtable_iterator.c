#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "ppdb/ppdb_logger.h"
#include "kvstore/internal/skiplist.h"
#include "kvstore/internal/sync.h"

// 创建迭代器
ppdb_error_t ppdb_memtable_iterator_create_basic(ppdb_memtable_t* table,
                                                ppdb_memtable_iterator_t** iter) {
    if (!table || !iter) return PPDB_ERR_INVALID_ARG;
    
    ppdb_memtable_iterator_t* new_iter = malloc(sizeof(ppdb_memtable_iterator_t));
    if (!new_iter) return PPDB_ERR_OUT_OF_MEMORY;

    new_iter->table = table;
    new_iter->current_shard = 0;
    new_iter->it = NULL;
    new_iter->valid = false;
    memset(&new_iter->current_pair, 0, sizeof(ppdb_kv_pair_t));

    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000
    };

    // 创建第一个分片的迭代器
    if (table->shards) {
        ppdb_error_t err = ppdb_skiplist_iterator_create(table->shards[0].basic->skiplist,
                                                       &new_iter->it,
                                                       &sync_config);
        if (err != PPDB_OK) {
            free(new_iter);
            return err;
        }
    } else {
        ppdb_error_t err = ppdb_skiplist_iterator_create(table->basic->skiplist,
                                                       &new_iter->it,
                                                       &sync_config);
        if (err != PPDB_OK) {
            free(new_iter);
            return err;
        }
    }

    *iter = new_iter;
    return PPDB_OK;
}

// 迭代器移动到下一个元素
ppdb_error_t ppdb_memtable_iterator_next_basic(ppdb_memtable_iterator_t* iter,
                                              ppdb_kv_pair_t** pair) {
    if (!iter || !pair) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (!iter->it) {
        return PPDB_ERR_NOT_FOUND;
    }

    ppdb_error_t err = ppdb_skiplist_iterator_next(iter->it, &iter->current_pair);
    if (err != PPDB_OK) {
        // 当前分片迭代完成，尝试下一个分片
        if (iter->table->shards) {
            ppdb_skiplist_iterator_destroy(iter->it);
            iter->it = NULL;

            iter->current_shard++;
            if (iter->current_shard >= iter->table->shard_count) {
                return PPDB_ERR_NOT_FOUND;  // 所有分片都迭代完成
            }

            ppdb_sync_config_t sync_config = {
                .type = PPDB_SYNC_MUTEX,
                .spin_count = 1000
            };

            err = ppdb_skiplist_iterator_create(
                iter->table->shards[iter->current_shard].basic->skiplist,
                &iter->it,
                &sync_config
            );

            if (err != PPDB_OK) {
                return err;
            }

            err = ppdb_skiplist_iterator_next(iter->it, &iter->current_pair);
            if (err != PPDB_OK) {
                return err;
            }
        } else {
            return err;
        }
    }

    *pair = &iter->current_pair;
    return PPDB_OK;
}

// 获取当前键值对
ppdb_error_t ppdb_memtable_iterator_get_basic(ppdb_memtable_iterator_t* iter,
                                             ppdb_kv_pair_t* pair) {
    if (!iter || !pair) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (!iter->it) {
        return PPDB_ERR_NOT_FOUND;
    }

    return ppdb_skiplist_iterator_get(iter->it, pair);
}

// 销毁迭代器
void ppdb_memtable_iterator_destroy_basic(ppdb_memtable_iterator_t* iter) {
    if (!iter) return;
    
    if (iter->it) {
        ppdb_skiplist_iterator_destroy(iter->it);
    }
    free(iter);
}