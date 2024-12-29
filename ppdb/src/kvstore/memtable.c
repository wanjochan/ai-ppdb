#include <cosmopolitan.h>
#include "internal/memtable.h"
#include "ppdb/error.h"
#include "ppdb/logger.h"
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

ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table) {
    if (!table) return PPDB_ERR_INVALID_ARG;

    ppdb_memtable_t* new_table = aligned_alloc(64, sizeof(ppdb_memtable_t));
    if (!new_table) return PPDB_ERR_NO_MEMORY;

    // 初始化同步原语
    ppdb_sync_config_t sync_config = PPDB_SYNC_DEFAULT_CONFIG;
    ppdb_sync_init(&new_table->sync, &sync_config);

    // 创建跳表
    ppdb_skiplist_config_t sl_config = {
        .max_level = 12,
        .sync_config = sync_config,
        .enable_hint = true
    };
    new_table->skiplist = ppdb_skiplist_create(&sl_config);
    if (!new_table->skiplist) {
        ppdb_sync_destroy(&new_table->sync);
        free(new_table);
        return PPDB_ERR_NO_MEMORY;
    }

    // 初始化性能监控
    ppdb_metrics_init(&new_table->metrics);

    new_table->max_size = size_limit;
    atomic_init(&new_table->current_size, 0);
    atomic_init(&new_table->is_immutable, false);

    *table = new_table;
    return PPDB_OK;
}

void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    if (!table) return;

    if (table->skiplist) {
        ppdb_skiplist_destroy(table->skiplist);
        table->skiplist = NULL;
    }

    ppdb_sync_destroy(&table->sync);
    ppdb_metrics_destroy(&table->metrics);
    free(table);
}

ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table, const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len) {
    if (!table || !key || !value) return PPDB_ERR_INVALID_ARG;

    // 记录操作开始
    ppdb_metrics_begin_op(&table->metrics);
    
    // 检查大小限制
    if (atomic_load(&table->current_size) >= table->max_size) {
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_FULL;
    }

    // 检查是否不可变
    if (atomic_load(&table->is_immutable)) {
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_IMMUTABLE;
    }

    // 尝试插入
    size_t entry_size = key_len + value_len + sizeof(ppdb_skiplist_node_t);
    int result = ppdb_skiplist_insert(table->skiplist, key, key_len, value, value_len);
    
    if (result == PPDB_OK) {
        atomic_fetch_add(&table->current_size, entry_size);
        ppdb_metrics_end_op(&table->metrics, 1);
        return PPDB_OK;
    }

    ppdb_metrics_end_op(&table->metrics, 0);
    return PPDB_ERR_INTERNAL;
}

ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table, const uint8_t* key, size_t key_len,
                              uint8_t** value, size_t* value_len) {
    if (!table || !key || !value || !value_len) return PPDB_ERR_INVALID_ARG;

    ppdb_metrics_begin_op(&table->metrics);

    int result = ppdb_skiplist_find(table->skiplist, key, key_len, 
                                  (void**)value, value_len);

    if (result == PPDB_OK) {
        ppdb_metrics_end_op(&table->metrics, 1);
        return PPDB_OK;
    } else if (result == PPDB_NOT_FOUND) {
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_NOT_FOUND;
    }

    ppdb_metrics_end_op(&table->metrics, 0);
    return PPDB_ERR_INTERNAL;
}

ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table, const uint8_t* key, size_t key_len) {
    if (!table || !key) return PPDB_ERR_INVALID_ARG;

    ppdb_metrics_begin_op(&table->metrics);

    // 检查是否不可变
    if (atomic_load(&table->is_immutable)) {
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_IMMUTABLE;
    }

    int result = ppdb_skiplist_remove(table->skiplist, key, key_len);
    
    if (result == PPDB_OK) {
        ppdb_metrics_end_op(&table->metrics, 1);
        return PPDB_OK;
    } else if (result == PPDB_NOT_FOUND) {
        ppdb_metrics_end_op(&table->metrics, 0);
        return PPDB_ERR_NOT_FOUND;
    }

    ppdb_metrics_end_op(&table->metrics, 0);
    return PPDB_ERR_INTERNAL;
}

size_t ppdb_memtable_size(ppdb_memtable_t* table) {
    if (!table) return 0;
    return atomic_load(&table->current_size);
}

size_t ppdb_memtable_max_size(ppdb_memtable_t* table) {
    if (!table) return 0;
    return table->max_size;
}

bool ppdb_memtable_is_immutable(ppdb_memtable_t* table) {
    if (!table) return true;
    return atomic_load(&table->is_immutable);
}

void ppdb_memtable_set_immutable(ppdb_memtable_t* table) {
    if (!table) return;
    atomic_store(&table->is_immutable, true);
}

ppdb_metrics_t ppdb_memtable_get_metrics(ppdb_memtable_t* table) {
    if (!table) return (ppdb_metrics_t){0};
    return table->metrics;
}
