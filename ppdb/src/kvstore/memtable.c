#include "internal/memtable.h"
#include "ppdb/error.h"
#include "ppdb/logger.h"
#include "internal/skiplist.h"
#include "internal/sync.h"

// MemTable结构
struct ppdb_memtable_t {
    skiplist_t* skiplist;     // 底层跳表
    ppdb_sync_t* sync;        // 同步原语
    size_t max_size;          // 最大大小
    size_t current_size;      // 当前大小
    bool is_immutable;        // 是否不可变
};

ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table) {
    if (!table) return PPDB_ERR_INVALID_ARG;

    ppdb_memtable_t* new_table = (ppdb_memtable_t*)malloc(sizeof(ppdb_memtable_t));
    if (!new_table) return PPDB_ERR_NO_MEMORY;

    // 创建同步原语
    ppdb_error_t err = ppdb_sync_create(&new_table->sync);
    if (err != PPDB_OK) {
        free(new_table);
        return err;
    }

    // 创建跳表
    new_table->skiplist = skiplist_create();
    if (!new_table->skiplist) {
        ppdb_sync_destroy(new_table->sync);
        free(new_table);
        return PPDB_ERR_NO_MEMORY;
    }

    new_table->max_size = size_limit;
    new_table->current_size = 0;
    new_table->is_immutable = false;

    *table = new_table;
    return PPDB_OK;
}

void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    if (!table) return;

    if (table->skiplist) {
        skiplist_destroy(table->skiplist);
        table->skiplist = NULL;
    }

    if (table->sync) {
        ppdb_sync_destroy(table->sync);
        table->sync = NULL;
    }

    free(table);
}

ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table, const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len) {
    if (!table || !key || !value) return PPDB_ERR_INVALID_ARG;

    // 检查大小限制
    if (ppdb_sync_load_size(table->sync, &table->current_size) >= table->max_size) {
        return PPDB_ERR_FULL;
    }

    // 加锁
    ppdb_sync_lock(table->sync);

    // 再次检查大小限制（可能在获取锁的过程中被其他线程写满）
    if (ppdb_sync_load_size(table->sync, &table->current_size) >= table->max_size) {
        ppdb_sync_unlock(table->sync);
        return PPDB_ERR_FULL;
    }

    // 检查是否不可变
    bool is_immutable;
    ppdb_sync_load_bool(table->sync, &table->is_immutable, &is_immutable);
    if (is_immutable) {
        ppdb_sync_unlock(table->sync);
        return PPDB_ERR_IMMUTABLE;
    }

    // 插入数据
    int ret = skiplist_put(table->skiplist, key, key_len, value, value_len);

    // 更新大小
    if (ret == 0) {
        ppdb_sync_add_size(table->sync, &table->current_size, key_len + value_len);
    }

    ppdb_sync_unlock(table->sync);
    return ret == 0 ? PPDB_OK : PPDB_ERR_INTERNAL;
}

ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table, const uint8_t* key, size_t key_len,
                              uint8_t** value, size_t* value_len) {
    if (!table || !key || !value || !value_len) return PPDB_ERR_INVALID_ARG;

    ppdb_sync_lock(table->sync);

    // 查找数据
    int ret = skiplist_get(table->skiplist, key, key_len, *value, value_len);

    ppdb_sync_unlock(table->sync);
    return ret == 0 ? PPDB_OK : PPDB_ERR_NOT_FOUND;
}

ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table, const uint8_t* key, size_t key_len) {
    if (!table || !key) return PPDB_ERR_INVALID_ARG;

    ppdb_sync_lock(table->sync);

    // 检查是否不可变
    bool is_immutable;
    ppdb_sync_load_bool(table->sync, &table->is_immutable, &is_immutable);
    if (is_immutable) {
        ppdb_sync_unlock(table->sync);
        return PPDB_ERR_IMMUTABLE;
    }

    // 删除数据
    int ret = skiplist_delete(table->skiplist, key, key_len);

    ppdb_sync_unlock(table->sync);
    return ret == 0 ? PPDB_OK : PPDB_ERR_NOT_FOUND;
}

size_t ppdb_memtable_size(ppdb_memtable_t* table) {
    if (!table) return 0;
    size_t size;
    ppdb_sync_load_size(table->sync, &table->current_size);
    return size;
}

size_t ppdb_memtable_max_size(ppdb_memtable_t* table) {
    if (!table) return 0;
    return table->max_size;
}

bool ppdb_memtable_is_immutable(ppdb_memtable_t* table) {
    if (!table) return false;
    bool is_immutable;
    ppdb_sync_load_bool(table->sync, &table->is_immutable, &is_immutable);
    return is_immutable;
}

void ppdb_memtable_set_immutable(ppdb_memtable_t* table) {
    if (!table) return;
    ppdb_sync_lock(table->sync);
    ppdb_sync_store_bool(table->sync, &table->is_immutable, true);
    ppdb_sync_unlock(table->sync);
}
