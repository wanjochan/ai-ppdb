#include "memtable_unified.h"
#include <stdlib.h>
#include <string.h>

// 内部工具函数
static inline void update_stats(ppdb_memtable_t* table, 
                              size_t delta, 
                              bool is_insert) {
    atomic_fetch_add(&table->stats.mem_used, delta);
    if (is_insert) {
        atomic_fetch_add(&table->stats.inserts, 1);
    } else {
        atomic_fetch_add(&table->stats.updates, 1);
    }
}

// 创建MemTable
ppdb_memtable_t* ppdb_memtable_create(const ppdb_memtable_config_t* config) {
    ppdb_memtable_t* table = calloc(1, sizeof(ppdb_memtable_t));
    if (!table) return NULL;
    
    // 复制配置
    memcpy(&table->config, config, sizeof(ppdb_memtable_config_t));
    
    // 初始化跳表
    ppdb_skiplist_config_t skiplist_config = {
        .sync_config = config->sync_config,
        .max_size = config->max_size,
        .max_level = config->max_level,
        .enable_hint = true
    };
    
    table->skiplist = ppdb_skiplist_create(&skiplist_config);
    if (!table->skiplist) {
        free(table);
        return NULL;
    }
    
    // 初始化布隆过滤器
    if (config->enable_bloom_filter) {
        // TODO: 实现布隆过滤器初始化
    }
    
    // 初始化压缩上下文
    if (config->enable_compression) {
        // TODO: 实现压缩上下文初始化
    }
    
    atomic_init(&table->is_immutable, false);
    return table;
}

void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    if (!table) return;
    
    if (table->skiplist) {
        ppdb_skiplist_destroy(table->skiplist);
    }
    
    if (table->opt.bloom_filter) {
        free(table->opt.bloom_filter);
    }
    
    if (table->opt.compress_ctx) {
        free(table->opt.compress_ctx);
    }
    
    free(table);
}

// 写入操作
int ppdb_memtable_put(ppdb_memtable_t* table,
                      const void* key, size_t key_len,
                      const void* value, size_t value_len) {
    // 检查是否可写
    if (atomic_load(&table->is_immutable)) {
        return PPDB_ERR_READONLY;
    }
    
    // 检查内存限制
    if (atomic_load(&table->stats.mem_used) + key_len + value_len > 
        table->config.max_size) {
        return PPDB_ERR_NO_MEMORY;
    }
    
    // 更新布隆过滤器
    if (table->config.enable_bloom_filter && table->opt.bloom_filter) {
        // TODO: 更新布隆过滤器
    }
    
    // 压缩值（如果启用）
    void* compressed_value = (void*)value;
    size_t compressed_len = value_len;
    if (table->config.enable_compression && table->opt.compress_ctx) {
        // TODO: 实现值压缩
    }
    
    // 写入跳表
    int ret = ppdb_skiplist_insert(table->skiplist, 
                                 key, key_len,
                                 compressed_value, compressed_len);
    
    if (ret == PPDB_OK) {
        update_stats(table, key_len + compressed_len, true);
    } else if (ret == PPDB_ERR_BUSY) {
        atomic_fetch_add(&table->stats.conflicts, 1);
    }
    
    return ret;
}

// 读取操作
int ppdb_memtable_get(ppdb_memtable_t* table,
                      const void* key, size_t key_len,
                      void** value, size_t* value_len) {
    // 检查布隆过滤器
    if (table->config.enable_bloom_filter && table->opt.bloom_filter) {
        // TODO: 检查布隆过滤器，如果确定不存在则直接返回
    }
    
    // 从跳表中查找
    int ret = ppdb_skiplist_find(table->skiplist,
                               key, key_len,
                               value, value_len);
    
    // 解压缩值（如果启用）
    if (ret == PPDB_OK && table->config.enable_compression && 
        table->opt.compress_ctx) {
        // TODO: 实现值解压缩
    }
    
    return ret;
}

// 删除操作
int ppdb_memtable_delete(ppdb_memtable_t* table,
                        const void* key, size_t key_len) {
    // 检查是否可写
    if (atomic_load(&table->is_immutable)) {
        return PPDB_ERR_READONLY;
    }
    
    int ret = ppdb_skiplist_remove(table->skiplist, key, key_len);
    
    if (ret == PPDB_OK) {
        atomic_fetch_add(&table->stats.deletes, 1);
    } else if (ret == PPDB_ERR_BUSY) {
        atomic_fetch_add(&table->stats.conflicts, 1);
    }
    
    return ret;
}

// 迭代器实现
ppdb_memtable_iter_t* ppdb_memtable_iter_create(ppdb_memtable_t* table) {
    ppdb_memtable_iter_t* iter = malloc(sizeof(ppdb_memtable_iter_t));
    if (!iter) return NULL;
    
    iter->table = table;
    iter->skiplist_iter = ppdb_skiplist_iter_create(table->skiplist);
    
    if (!iter->skiplist_iter) {
        free(iter);
        return NULL;
    }
    
    return iter;
}

void ppdb_memtable_iter_destroy(ppdb_memtable_iter_t* iter) {
    if (!iter) return;
    if (iter->skiplist_iter) {
        ppdb_skiplist_iter_destroy(iter->skiplist_iter);
    }
    free(iter);
}

bool ppdb_memtable_iter_valid(ppdb_memtable_iter_t* iter) {
    return iter && iter->skiplist_iter && 
           ppdb_skiplist_iter_valid(iter->skiplist_iter);
}

void ppdb_memtable_iter_next(ppdb_memtable_iter_t* iter) {
    if (iter && iter->skiplist_iter) {
        ppdb_skiplist_iter_next(iter->skiplist_iter);
    }
}

const void* ppdb_memtable_iter_key(ppdb_memtable_iter_t* iter, size_t* len) {
    if (!iter || !iter->skiplist_iter) return NULL;
    return ppdb_skiplist_iter_key(iter->skiplist_iter, len);
}

const void* ppdb_memtable_iter_value(ppdb_memtable_iter_t* iter, size_t* len) {
    if (!iter || !iter->skiplist_iter) return NULL;
    return ppdb_skiplist_iter_value(iter->skiplist_iter, len);
}

// 状态管理
void ppdb_memtable_make_immutable(ppdb_memtable_t* table) {
    atomic_store(&table->is_immutable, true);
}

bool ppdb_memtable_is_immutable(ppdb_memtable_t* table) {
    return atomic_load(&table->is_immutable);
}

size_t ppdb_memtable_memory_usage(ppdb_memtable_t* table) {
    return atomic_load(&table->stats.mem_used);
}
