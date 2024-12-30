#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "internal/kvstore_types.h"
#include "internal/kvstore_memtable.h"
#include "internal/kvstore_logger.h"
#include "internal/kvstore_fs.h"
#include "internal/skiplist.h"
#include "internal/sync.h"
#include "internal/metrics.h"

// MemTable structure
struct ppdb_memtable_t {
    ppdb_skiplist_t* skiplist;     // 底层跳表
    ppdb_sync_t sync;              // 同步原语
    atomic_size_t current_size;    // 当前大小
    size_t max_size;              // 最大大小
    atomic_bool is_immutable;     // 是否不可变
    ppdb_metrics_t metrics;       // 性能监控
};

// 创建内存表
ppdb_error_t ppdb_memtable_create(size_t size, ppdb_memtable_t** table) {
    if (!table) return PPDB_ERR_NULL_POINTER;
    if (size == 0) return PPDB_ERR_INVALID_ARG;

    // 分配内存表结构
    ppdb_memtable_t* new_table = aligned_alloc(64, sizeof(ppdb_memtable_t));
    if (!new_table) return PPDB_ERR_NO_MEMORY;

    // 初始化同步原语
    ppdb_error_t err = ppdb_sync_init(&new_table->sync);
    if (err != PPDB_OK) {
        free(new_table);
        return err;
    }

    // 创建跳表
    err = ppdb_skiplist_create(&new_table->skiplist);
    if (err != PPDB_OK) {
        ppdb_sync_destroy(&new_table->sync);
        free(new_table);
        return err;
    }

    // 初始化其他字段
    new_table->size = size;
    new_table->used = 0;
    new_table->is_immutable = false;
    ppdb_metrics_init(&new_table->metrics);

    *table = new_table;
    return PPDB_OK;
}

// 销毁内存表
void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    if (!table) return;

    ppdb_skiplist_destroy(table->skiplist);
    ppdb_sync_destroy(&table->sync);
    ppdb_metrics_destroy(&table->metrics);
    free(table);
}

// 写入键值对
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              const void* value, size_t value_len) {
    if (!table || !key || !value) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0 || value_len == 0) return PPDB_ERR_INVALID_ARG;

    // 开始监控
    ppdb_metrics_begin_op(&table->metrics);

    // 检查是否可写
    if (table->is_immutable) {
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_IMMUTABLE;
    }

    // 检查空间
    size_t entry_size = key_len + value_len + sizeof(ppdb_skiplist_node_t);
    if (table->used + entry_size > table->size) {
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_FULL;
    }

    // 写入跳表
    ppdb_error_t err = ppdb_skiplist_put(table->skiplist,
                                        (const uint8_t*)key, key_len,
                                        (const uint8_t*)value, value_len);
    if (err == PPDB_OK) {
        atomic_fetch_add(&table->used, entry_size);
    }

    ppdb_metrics_end_op(&table->metrics, now_us());
    return err;
}

// 读取键值对
ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              void** value, size_t* value_len) {
    if (!table || !key || !value || !value_len) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARG;

    // 开始监控
    ppdb_metrics_begin_op(&table->metrics);

    // 从跳表读取
    ppdb_error_t err = ppdb_skiplist_get(table->skiplist,
                                        (const uint8_t*)key, key_len,
                                        (uint8_t**)value, value_len);

    ppdb_metrics_end_op(&table->metrics, now_us());
    return err;
}

// 删除键值对
ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const void* key, size_t key_len) {
    if (!table || !key) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARG;

    // 开始监控
    ppdb_metrics_begin_op(&table->metrics);

    // 检查是否可写
    if (table->is_immutable) {
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_IMMUTABLE;
    }

    // 从跳表删除
    ppdb_error_t err = ppdb_skiplist_delete(table->skiplist,
                                           (const uint8_t*)key, key_len);

    ppdb_metrics_end_op(&table->metrics, now_us());
    return err;
}

// 获取当前大小
size_t ppdb_memtable_size(ppdb_memtable_t* table) {
    if (!table) return 0;
    return atomic_load(&table->used);
}

// 获取最大大小
size_t ppdb_memtable_max_size(ppdb_memtable_t* table) {
    if (!table) return 0;
    return table->size;
}

// 检查是否不可变
bool ppdb_memtable_is_immutable(ppdb_memtable_t* table) {
    if (!table) return false;
    return atomic_load(&table->is_immutable);
}

// 设置为不可变
ppdb_error_t ppdb_memtable_set_immutable(ppdb_memtable_t* table) {
    if (!table) return PPDB_ERR_NULL_POINTER;
    atomic_store(&table->is_immutable, true);
    return PPDB_OK;
}

// 获取性能指标
const ppdb_metrics_t* ppdb_memtable_get_metrics(ppdb_memtable_t* table) {
    if (!table) return NULL;
    return &table->metrics;
}
