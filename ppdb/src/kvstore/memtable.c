#include <cosmopolitan.h>
#include "ppdb/memtable.h"
#include "ppdb/error.h"
#include "../common/logger.h"
#include "skiplist.h"

// MemTable 结构
struct ppdb_memtable_t {
    size_t size_limit;      // 大小限制
    size_t current_size;    // 当前大小
    skiplist_t* list;       // 跳表
    pthread_mutex_t mutex;  // 互斥锁
};

// 创建 MemTable
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table) {
    if (!table) {
        return PPDB_ERR_NULL_POINTER;
    }

    ppdb_memtable_t* new_table = (ppdb_memtable_t*)malloc(sizeof(ppdb_memtable_t));
    if (!new_table) {
        return PPDB_ERR_NO_MEMORY;
    }

    new_table->size_limit = size_limit;
    new_table->current_size = 0;
    new_table->list = skiplist_create();
    if (!new_table->list) {
        free(new_table);
        return PPDB_ERR_NO_MEMORY;
    }

    if (pthread_mutex_init(&new_table->mutex, NULL) != 0) {
        skiplist_destroy(new_table->list);
        free(new_table);
        return PPDB_ERR_MUTEX_ERROR;
    }

    *table = new_table;
    return PPDB_OK;
}

// 销毁 MemTable
void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    if (!table) return;

    pthread_mutex_destroy(&table->mutex);
    skiplist_destroy(table->list);
    free(table);
}

// 写入键值对
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table,
                              const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len) {
    if (!table || !key || !value) {
        return PPDB_ERR_NULL_POINTER;
    }

    pthread_mutex_lock(&table->mutex);

    // 检查大小限制
    size_t entry_size = key_len + value_len;  // 实际数据大小
    if (table->current_size + entry_size > table->size_limit) {
        ppdb_log_warn("MemTable size limit exceeded: current=%zu, limit=%zu, new_entry=%zu",
                     table->current_size, table->size_limit, entry_size);
        pthread_mutex_unlock(&table->mutex);
        return PPDB_ERR_FULL;
    }

    // 插入跳表
    int ret = skiplist_put(table->list, key, key_len, value, value_len);
    if (ret != 0) {
        pthread_mutex_unlock(&table->mutex);
        return PPDB_ERR_NO_MEMORY;
    }

    // 更新当前大小
    table->current_size += entry_size;

    pthread_mutex_unlock(&table->mutex);
    return PPDB_OK;
}

// 读取键值对
ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const uint8_t* key, size_t key_len,
                              uint8_t* value, size_t* value_len) {
    if (!table || !key || !value || !value_len) {
        return PPDB_ERR_NULL_POINTER;
    }

    pthread_mutex_lock(&table->mutex);

    // 查找键
    int ret = skiplist_get(table->list, key, key_len, value, value_len);
    if (ret == 1) {
        pthread_mutex_unlock(&table->mutex);
        return PPDB_ERR_NOT_FOUND;
    } else if (ret != 0) {
        pthread_mutex_unlock(&table->mutex);
        return PPDB_ERR_NO_MEMORY;
    }

    pthread_mutex_unlock(&table->mutex);
    return PPDB_OK;
}

// 删除键值对
ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const uint8_t* key, size_t key_len) {
    if (!table || !key) {
        return PPDB_ERR_NULL_POINTER;
    }

    pthread_mutex_lock(&table->mutex);

    // 查找并删除键
    int ret = skiplist_delete(table->list, key, key_len);
    if (ret == 1) {
        pthread_mutex_unlock(&table->mutex);
        return PPDB_ERR_NOT_FOUND;
    } else if (ret != 0) {
        pthread_mutex_unlock(&table->mutex);
        return PPDB_ERR_NO_MEMORY;
    }

    // 更新当前大小（这里我们使用一个估计值，因为我们无法获取实际的键值大小）
    table->current_size = table->current_size > key_len ? table->current_size - key_len : 0;

    pthread_mutex_unlock(&table->mutex);
    return PPDB_OK;
} 