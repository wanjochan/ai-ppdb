#include <cosmopolitan.h>
#include "ppdb/memtable.h"
#include "ppdb/error.h"
#include "ppdb/logger.h"
#include "ppdb/atomic_skiplist.h"

// 无锁MemTable结构
struct ppdb_memtable_t {
    size_t size_limit;      // 大小限制
    atomic_size_t current_size;  // 当前大小
    atomic_skiplist_t* list;    // 原子跳表
};

ppdb_error_t ppdb_memtable_create_lockfree(size_t size_limit, ppdb_memtable_t** table) {
    if (!table) return PPDB_ERR_NULL_POINTER;

    ppdb_memtable_t* new_table = (ppdb_memtable_t*)malloc(sizeof(ppdb_memtable_t));
    if (!new_table) return PPDB_ERR_NO_MEMORY;

    new_table->size_limit = size_limit;
    atomic_init(&new_table->current_size, 0);
    new_table->list = atomic_skiplist_create();
    if (!new_table->list) {
        free(new_table);
        return PPDB_ERR_NO_MEMORY;
    }

    *table = new_table;
    return PPDB_OK;
}

void ppdb_memtable_destroy_lockfree(ppdb_memtable_t* table) {
    if (!table) return;
    atomic_skiplist_destroy(table->list);
    free(table);
}

ppdb_error_t ppdb_memtable_put_lockfree(ppdb_memtable_t* table,
                                       const uint8_t* key, size_t key_len,
                                       const uint8_t* value, size_t value_len) {
    if (!table || !key || !value) return PPDB_ERR_NULL_POINTER;

    size_t entry_size = key_len + value_len + sizeof(size_t) * 2;
    size_t old_size, new_size;

    // 先获取旧值的大小(如果存在)
    uint8_t temp_value[1];
    size_t old_value_size = 0;
    int get_ret = atomic_skiplist_get(table->list, key, key_len, temp_value, &old_value_size);
    
    do {
        old_size = atomic_load_explicit(&table->current_size, memory_order_acquire);
        if (get_ret == 0) {
            // 如果键已存在，减去旧值的大小
            size_t old_entry_size = key_len + old_value_size + sizeof(size_t) * 2;
            new_size = old_size - old_entry_size + entry_size;
        } else {
            new_size = old_size + entry_size;
        }

        if (new_size > table->size_limit) {
            ppdb_log_warn("MemTable size limit exceeded: current=%zu, limit=%zu, new_entry=%zu",
                         old_size, table->size_limit, entry_size);
            return PPDB_ERR_FULL;
        }
    } while (!atomic_compare_exchange_weak_explicit(&table->current_size, &old_size, new_size,
                                                  memory_order_release, memory_order_relaxed));

    int ret = atomic_skiplist_put(table->list, key, key_len, value, value_len);
    if (ret != 0) {
        // 回滚大小更新
        atomic_fetch_sub_explicit(&table->current_size, entry_size, memory_order_release);
        return PPDB_ERR_NO_MEMORY;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_memtable_get_lockfree(ppdb_memtable_t* table,
                                       const uint8_t* key, size_t key_len,
                                       uint8_t** value, size_t* value_len) {
    if (!table || !key || !value_len) return PPDB_ERR_NULL_POINTER;

    // 先查询值的大小
    size_t required_size = 0;
    int ret = atomic_skiplist_get(table->list, key, key_len, NULL, &required_size);
    if (ret == 1) return PPDB_ERR_NOT_FOUND;
    if (ret != 0) return PPDB_ERR_NO_MEMORY;

    // 如果只是查询大小
    if (!value) {
        *value_len = required_size;
        return PPDB_OK;
    }

    // 分配内存
    *value = (uint8_t*)malloc(required_size);
    if (!*value) return PPDB_ERR_NO_MEMORY;

    // 获取值
    size_t actual_size = required_size;
    ret = atomic_skiplist_get(table->list, key, key_len, *value, &actual_size);
    if (ret != 0) {
        free(*value);
        *value = NULL;
        return ret == 1 ? PPDB_ERR_NOT_FOUND : PPDB_ERR_NO_MEMORY;
    }

    *value_len = actual_size;
    return PPDB_OK;
}

ppdb_error_t ppdb_memtable_delete_lockfree(ppdb_memtable_t* table,
                                          const uint8_t* key, size_t key_len) {
    if (!table || !key) return PPDB_ERR_NULL_POINTER;

    // 先获取值的大小
    uint8_t temp_value[1];
    size_t value_size = 0;
    int get_ret = atomic_skiplist_get(table->list, key, key_len, temp_value, &value_size);
    if (get_ret != 0) return PPDB_ERR_NOT_FOUND;  // 任何非0返回值都表示未找到或错误

    // 删除键值对
    int ret = atomic_skiplist_delete(table->list, key, key_len);
    if (ret != 0) return PPDB_ERR_NOT_FOUND;  // 任何非0返回值都表示未找到或错误

    // 更新当前大小
    size_t entry_size = key_len + value_size + sizeof(size_t) * 2;
    size_t current = atomic_load_explicit(&table->current_size, memory_order_acquire);
    if (current >= entry_size) {
        atomic_fetch_sub_explicit(&table->current_size, entry_size, memory_order_release);
    } else {
        atomic_store_explicit(&table->current_size, 0, memory_order_release);
    }

    return PPDB_OK;
}

size_t ppdb_memtable_size_lockfree(ppdb_memtable_t* table) {
    if (!table) return 0;
    return atomic_load_explicit(&table->current_size, memory_order_relaxed);
}

size_t ppdb_memtable_max_size_lockfree(ppdb_memtable_t* table) {
    if (!table) return 0;
    return table->size_limit;
} 