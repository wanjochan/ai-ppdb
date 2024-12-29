#include <cosmopolitan.h>
#include "ppdb/memtable_mutex.h"
#include "ppdb/error.h"
#include "ppdb/logger.h"
#include "ppdb/skiplist_mutex.h"

// MemTable 结构
struct ppdb_memtable_t {
    size_t size_limit;      // 大小限制
    size_t current_size;    // 当前大小
    skiplist_t* list;       // 跳表
    pthread_mutex_t mutex;  // 互斥锁
};

// MemTable 迭代器结构
struct ppdb_memtable_iterator_t {
    ppdb_memtable_t* table;         // MemTable 指针
    skiplist_iterator_t* list_iter;  // 跳表迭代器
    uint8_t* current_key;           // 当前键
    uint8_t* current_value;         // 当前值
    size_t current_key_size;        // 当前键大小
    size_t current_value_size;      // 当前值大小
};

// 创建 MemTable
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table) {
    if (!table) {
        ppdb_log_error("Invalid argument: table is NULL");
        return PPDB_ERR_NULL_POINTER;
    }

    if (size_limit == 0) {
        ppdb_log_error("Invalid argument: size_limit is 0");
        return PPDB_ERR_INVALID_ARG;
    }

    // Allocate and zero initialize the memtable structure
    ppdb_memtable_t* new_table = (ppdb_memtable_t*)calloc(1, sizeof(ppdb_memtable_t));
    if (!new_table) {
        ppdb_log_error("Failed to allocate memtable");
        return PPDB_ERR_NO_MEMORY;
    }

    // Initialize fields first
    new_table->size_limit = size_limit;
    new_table->current_size = 0;
    new_table->list = NULL;

    // Initialize mutex
    if (pthread_mutex_init(&new_table->mutex, NULL) != 0) {
        ppdb_log_error("Failed to initialize mutex");
        free(new_table);
        return PPDB_ERR_MUTEX_ERROR;
    }

    // Create skiplist
    new_table->list = skiplist_create();
    if (!new_table->list) {
        ppdb_log_error("Failed to create skiplist");
        pthread_mutex_destroy(&new_table->mutex);
        free(new_table);
        return PPDB_ERR_NO_MEMORY;
    }

    *table = new_table;
    ppdb_log_debug("Created memtable with size limit: %zu", size_limit);
    return PPDB_OK;
}

// 关闭 MemTable（保留数据）
void ppdb_memtable_close(ppdb_memtable_t* table) {
    if (!table) return;

    // 先加锁
    pthread_mutex_lock(&table->mutex);

    // 保存指针
    skiplist_t* list = table->list;
    table->list = NULL;
    table->current_size = 0;
    table->size_limit = 0;

    // 解锁并销毁互斥锁
    pthread_mutex_unlock(&table->mutex);
    pthread_mutex_destroy(&table->mutex);

    // 销毁skiplist
    if (list) {
        skiplist_destroy(list);
    }

    // 清零整个结构体
    memset(table, 0, sizeof(ppdb_memtable_t));

    // 释放结构体
    free(table);
}

// 销毁 MemTable（释放所有资源）
void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    if (!table) return;

    // 先加锁
    pthread_mutex_lock(&table->mutex);

    // 保存指针
    skiplist_t* list = table->list;
    table->list = NULL;
    table->current_size = 0;
    table->size_limit = 0;

    // 解锁并销毁互斥锁
    pthread_mutex_unlock(&table->mutex);
    pthread_mutex_destroy(&table->mutex);

    // 销毁skiplist
    if (list) {
        skiplist_destroy(list);
    }

    // 清零整个结构体
    memset(table, 0, sizeof(ppdb_memtable_t));

    // 释放结构体
    free(table);
}

// 写入键值对
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table,
                              const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len) {
    if (!table || !key || !value) {
        return PPDB_ERR_NULL_POINTER;
    }

    if (key_len == 0 || value_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&table->mutex);

    // 检查大小限制
    size_t entry_size = key_len + value_len + sizeof(size_t) * 2;  // 加上长度字段的大小
    size_t new_size = table->current_size + entry_size;

    // 先获取旧值的大小(如果存在)
    size_t old_value_size = 0;
    ppdb_error_t get_ret = skiplist_get(table->list, key, key_len, NULL, &old_value_size);
    if (get_ret == PPDB_OK) {
        // 如果键已存在,减去旧值的大小
        size_t old_entry_size = key_len + old_value_size + sizeof(size_t) * 2;
        new_size = table->current_size - old_entry_size + entry_size;
    }

    if (new_size > table->size_limit) {
        ppdb_log_warn("MemTable size limit exceeded: current=%zu, limit=%zu, new_entry=%zu",
                     table->current_size, table->size_limit, entry_size);
        pthread_mutex_unlock(&table->mutex);
        return PPDB_ERR_FULL;
    }

    // 插入跳表
    ppdb_error_t ret = skiplist_put(table->list, key, key_len, value, value_len);
    if (ret != PPDB_OK) {
        pthread_mutex_unlock(&table->mutex);
        return ret;  // 返回具体的错误码
    }

    // 更新当前大小
    table->current_size = new_size;

    pthread_mutex_unlock(&table->mutex);
    return PPDB_OK;
}

// 读取键值对
ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const uint8_t* key, size_t key_len,
                              uint8_t** value, size_t* value_len) {
    if (!table || !key || !value_len) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&table->mutex);

    // 先查询值的大小
    size_t required_size = 0;
    ppdb_error_t ret = skiplist_get(table->list, key, key_len, NULL, &required_size);
    if (ret == PPDB_ERR_NOT_FOUND) {
        pthread_mutex_unlock(&table->mutex);
        return PPDB_ERR_NOT_FOUND;
    } else if (ret != PPDB_OK) {
        pthread_mutex_unlock(&table->mutex);
        return ret;
    }

    // 如果只是查询大小
    if (!value) {
        *value_len = required_size;
        pthread_mutex_unlock(&table->mutex);
        return PPDB_OK;
    }

    // 获取值
    ret = skiplist_get(table->list, key, key_len, value, value_len);
    if (ret != PPDB_OK) {
        pthread_mutex_unlock(&table->mutex);
        return ret;
    }

    pthread_mutex_unlock(&table->mutex);
    return PPDB_OK;
}

// 删除键值对
ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const uint8_t* key, size_t key_len) {
    if (!table || !key) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&table->mutex);

    // 删除键值对
    ppdb_error_t ret = skiplist_delete(table->list, key, key_len);
    if (ret == PPDB_OK) {
        // 更新当前大小
        size_t value_size = 0;
        ppdb_error_t size_err = skiplist_get(table->list, key, key_len, NULL, &value_size);
        if (size_err == PPDB_OK) {
            // 如果键仍然存在,说明删除失败
            ret = PPDB_ERR_INTERNAL;
            ppdb_log_error("Key still exists after deletion in memtable");
        } else {
            // 更新当前大小，包括键值对的总大小
            size_t entry_size = key_len + value_size + sizeof(size_t) * 2;
            if (table->current_size >= entry_size) {
                table->current_size -= entry_size;
            } else {
                table->current_size = 0;
            }
        }
    }

    pthread_mutex_unlock(&table->mutex);
    return ret;
}

// 获取MemTable的当前大小
size_t ppdb_memtable_size(ppdb_memtable_t* table) {
    if (!table) {
        return 0;
    }
    return table->current_size;
}

// 获取MemTable的最大大小
size_t ppdb_memtable_max_size(ppdb_memtable_t* table) {
    if (!table) {
        return 0;
    }
    return table->size_limit;
}

// 复制数据到新的MemTable
ppdb_error_t ppdb_memtable_copy(ppdb_memtable_t* src, ppdb_memtable_t* dst) {
    if (!src || !dst) {
        return PPDB_ERR_NULL_POINTER;
    }

    pthread_mutex_lock(&src->mutex);
    pthread_mutex_lock(&dst->mutex);

    // 获取源MemTable的所有键值对
    skiplist_iterator_t* iter = skiplist_iterator_create(src->list);
    if (!iter) {
        pthread_mutex_unlock(&dst->mutex);
        pthread_mutex_unlock(&src->mutex);
        return PPDB_ERR_NO_MEMORY;
    }

    ppdb_error_t err = PPDB_OK;
    uint8_t* key;
    uint8_t* value;
    size_t key_size;
    size_t value_size;

    // 遍历并复制每个键值对
    while (skiplist_iterator_next(iter, &key, &key_size, &value, &value_size)) {
        err = ppdb_memtable_put(dst, key, key_size, value, value_size);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to copy key-value pair: %d", err);
            break;
        }
    }

    skiplist_iterator_destroy(iter);
    pthread_mutex_unlock(&dst->mutex);
    pthread_mutex_unlock(&src->mutex);
    return err;
}

// 创建迭代器
ppdb_error_t ppdb_memtable_iterator_create(ppdb_memtable_t* table, ppdb_memtable_iterator_t** iter) {
    if (!table || !iter) {
        return PPDB_ERR_NULL_POINTER;
    }

    ppdb_memtable_iterator_t* new_iter = (ppdb_memtable_iterator_t*)malloc(sizeof(ppdb_memtable_iterator_t));
    if (!new_iter) {
        return PPDB_ERR_NO_MEMORY;
    }

    new_iter->table = table;
    new_iter->list_iter = skiplist_iterator_create(table->list);
    if (!new_iter->list_iter) {
        free(new_iter);
        return PPDB_ERR_NO_MEMORY;
    }

    new_iter->current_key = NULL;
    new_iter->current_value = NULL;
    new_iter->current_key_size = 0;
    new_iter->current_value_size = 0;

    // 移动到第一个元素
    if (!skiplist_iterator_next(new_iter->list_iter,
                              &new_iter->current_key, &new_iter->current_key_size,
                              &new_iter->current_value, &new_iter->current_value_size)) {
        skiplist_iterator_destroy(new_iter->list_iter);
        free(new_iter);
        return PPDB_ERR_NOT_FOUND;
    }

    *iter = new_iter;
    return PPDB_OK;
}

// 销毁迭代器
void ppdb_memtable_iterator_destroy(ppdb_memtable_iterator_t* iter) {
    if (!iter) return;

    if (iter->list_iter) {
        skiplist_iterator_destroy(iter->list_iter);
    }
    free(iter);
}

// 检查迭代器是否有效
bool ppdb_memtable_iterator_valid(ppdb_memtable_iterator_t* iter) {
    if (!iter) return false;
    return iter->current_key != NULL;
}

// 获取当前键
const uint8_t* ppdb_memtable_iterator_key(ppdb_memtable_iterator_t* iter) {
    if (!iter) return NULL;
    return iter->current_key;
}

// 获取当前值
const uint8_t* ppdb_memtable_iterator_value(ppdb_memtable_iterator_t* iter) {
    if (!iter) return NULL;
    return iter->current_value;
}

// 移动到下一个位置
void ppdb_memtable_iterator_next(ppdb_memtable_iterator_t* iter) {
    if (!iter) return;

    if (!skiplist_iterator_next(iter->list_iter,
                              &iter->current_key, &iter->current_key_size,
                              &iter->current_value, &iter->current_value_size)) {
        iter->current_key = NULL;
        iter->current_value = NULL;
        iter->current_key_size = 0;
        iter->current_value_size = 0;
    }
}

// 获取当前键值对
ppdb_error_t ppdb_memtable_iterator_get(ppdb_memtable_iterator_t* iter,
                                       const uint8_t** key, size_t* key_len,
                                       const uint8_t** value, size_t* value_len) {
    if (!iter || !key || !key_len || !value || !value_len) {
        return PPDB_ERR_NULL_POINTER;
    }

    if (!iter->current_key) {
        return PPDB_ERR_NOT_FOUND;
    }

    *key = iter->current_key;
    *key_len = iter->current_key_size;
    *value = iter->current_value;
    *value_len = iter->current_value_size;

    return PPDB_OK;
}