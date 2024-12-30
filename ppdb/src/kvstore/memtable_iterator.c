#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_logger.h"
#include "kvstore/internal/skiplist.h"
#include "kvstore/internal/sync.h"

// 创建迭代器
ppdb_error_t ppdb_memtable_iterator_create_basic(ppdb_memtable_t* table,
                                                ppdb_memtable_iterator_t** iter) {
    if (!table || !iter) return PPDB_ERR_INVALID_ARG;
    
    ppdb_memtable_iterator_t* new_iter = malloc(sizeof(ppdb_memtable_iterator_t));
    if (!new_iter) return PPDB_ERR_OUT_OF_MEMORY;

    new_iter->table = table;
    new_iter->it = NULL;
    new_iter->valid = false;
    memset(&new_iter->current_pair, 0, sizeof(ppdb_kv_pair_t));

    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000
    };

    ppdb_error_t err = ppdb_skiplist_iterator_create(table->basic->skiplist,
                                                   &new_iter->it,
                                                   &sync_config);
    if (err != PPDB_OK) {
        free(new_iter);
        return err;
    }

    *iter = new_iter;
    return PPDB_OK;
}

// 迭代器移动到下一个元素
ppdb_error_t ppdb_memtable_iterator_next_basic(ppdb_memtable_iterator_t* iter,
                                              void** key, size_t* key_len,
                                              void** value, size_t* value_len) {
    if (!iter || !key || !key_len || !value || !value_len) {
        return PPDB_ERR_INVALID_ARG;
    }

    void* tmp_key;
    void* tmp_value;
    ppdb_error_t err = ppdb_skiplist_iterator_next(iter->it,
                                                  &tmp_key, key_len,
                                                  &tmp_value, value_len);
    if (err == PPDB_OK) {
        *key = tmp_key;
        *value = tmp_value;
        iter->valid = true;
        iter->current_pair.key = tmp_key;
        iter->current_pair.key_size = *key_len;
        iter->current_pair.value = tmp_value;
        iter->current_pair.value_size = *value_len;
    } else {
        iter->valid = false;
    }
    return err;
}

// 销毁迭代器
void ppdb_memtable_iterator_destroy_basic(ppdb_memtable_iterator_t* iter) {
    if (!iter) return;
    
    if (iter->it) {
        ppdb_skiplist_iterator_destroy(iter->it);
    }
    free(iter);
} 