#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_logger.h"
#include "kvstore/internal/skiplist.h"
#include "kvstore/internal/sync.h"

// 内存表迭代器结构
struct ppdb_memtable_iterator {
    ppdb_memtable_t* table;
    ppdb_skiplist_iterator_t* skiplist_iter;
};

// 创建迭代器
ppdb_error_t ppdb_memtable_iterator_create_basic(ppdb_memtable_t* table,
                                                ppdb_memtable_iterator_t** iter) {
    if (!table || !iter) return PPDB_ERR_INVALID_ARG;
    
    ppdb_memtable_iterator_t* new_iter = malloc(sizeof(ppdb_memtable_iterator_t));
    if (!new_iter) return PPDB_ERR_OUT_OF_MEMORY;

    new_iter->table = table;
    new_iter->skiplist_iter = NULL;

    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000
    };

    ppdb_error_t err = ppdb_skiplist_iterator_create(table->skiplist,
                                                   &new_iter->skiplist_iter,
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

    uint8_t* tmp_key;
    uint8_t* tmp_value;
    ppdb_error_t err = ppdb_skiplist_iterator_next(iter->skiplist_iter,
                                                  &tmp_key, key_len,
                                                  &tmp_value, value_len);
    if (err == PPDB_OK) {
        *key = tmp_key;
        *value = tmp_value;
    }
    return err;
}

// 销毁迭代器
void ppdb_memtable_iterator_destroy_basic(ppdb_memtable_iterator_t* iter) {
    if (!iter) return;
    
    if (iter->skiplist_iter) {
        ppdb_skiplist_iterator_destroy(iter->skiplist_iter);
    }
    free(iter);
} 