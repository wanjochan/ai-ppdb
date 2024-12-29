#include "ppdb/memtable/memtable.h"
#include "ppdb/skiplist/skiplist.h"
#include <stdlib.h>
#include <string.h>

// MemTable结构
struct ppdb_memtable {
    ppdb_skiplist_t* skiplist;     // 底层跳表
    ppdb_sync_t sync;              // 同步机制
    bool is_immutable;             // 是否不可变
    size_t max_size;               // 最大大小
    bool enable_compression;        // 启用压缩
    bool enable_bloom_filter;       // 启用布隆过滤器
    
    struct {
        size_t size;               // 数据条数
        size_t memory_usage;       // 内存使用量
        size_t compressed_size;    // 压缩后大小
    } stats;
};

// 创建MemTable
ppdb_memtable_t* ppdb_memtable_create(const ppdb_memtable_config_t* config) {
    if (!config) return NULL;

    ppdb_memtable_t* table = (ppdb_memtable_t*)malloc(sizeof(ppdb_memtable_t));
    if (!table) return NULL;

    // 初始化同步机制
    if (ppdb_sync_init(&table->sync, &config->sync_config) != 0) {
        free(table);
        return NULL;
    }

    // 创建跳表
    ppdb_skiplist_config_t skiplist_config = {
        .sync_config = config->sync_config,
        .enable_hint = true,
        .max_size = config->max_size,
        .max_level = config->max_level
    };

    table->skiplist = ppdb_skiplist_create(&skiplist_config);
    if (!table->skiplist) {
        ppdb_sync_destroy(&table->sync);
        free(table);
        return NULL;
    }

    table->is_immutable = false;
    table->max_size = config->max_size;
    table->enable_compression = config->enable_compression;
    table->enable_bloom_filter = config->enable_bloom_filter;
    memset(&table->stats, 0, sizeof(table->stats));

    return table;
}

// 销毁MemTable
void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    if (!table) return;

    if (table->skiplist) {
        ppdb_skiplist_destroy(table->skiplist);
    }
    ppdb_sync_destroy(&table->sync);
    free(table);
}

// 写入数据
int ppdb_memtable_put(ppdb_memtable_t* table, const void* key, size_t key_len,
                      const void* value, size_t value_len) {
    if (!table || !key || !value) return PPDB_ERROR;
    if (table->is_immutable) return PPDB_ERROR;

    ppdb_sync_lock(&table->sync);

    // 检查大小限制
    if (table->stats.memory_usage + key_len + value_len > table->max_size) {
        ppdb_sync_unlock(&table->sync);
        return PPDB_FULL;
    }

    // TODO: 压缩value
    void* compressed_value = NULL;
    size_t compressed_len = value_len;
    if (table->enable_compression) {
        // 实现压缩逻辑
    }

    // 写入跳表
    int ret = ppdb_skiplist_insert(table->skiplist, key, key_len,
                                 compressed_value ? compressed_value : value,
                                 compressed_len);

    if (compressed_value) {
        free(compressed_value);
    }

    if (ret == PPDB_OK) {
        table->stats.size++;
        table->stats.memory_usage += key_len + compressed_len;
        if (table->enable_compression) {
            table->stats.compressed_size += compressed_len;
        }
    }

    ppdb_sync_unlock(&table->sync);
    return ret;
}

// 读取数据
int ppdb_memtable_get(ppdb_memtable_t* table, const void* key, size_t key_len,
                      void** value, size_t* value_len) {
    if (!table || !key || !value || !value_len) return PPDB_ERROR;

    // TODO: 检查布隆过滤器
    if (table->enable_bloom_filter) {
        // 如果布隆过滤器表明key不存在，直接返回
    }

    ppdb_sync_lock(&table->sync);

    int ret = ppdb_skiplist_find(table->skiplist, key, key_len, value, value_len);

    // TODO: 解压缩value
    if (ret == PPDB_OK && table->enable_compression) {
        // 实现解压缩逻辑
    }

    ppdb_sync_unlock(&table->sync);
    return ret;
}

// 删除数据
int ppdb_memtable_delete(ppdb_memtable_t* table, const void* key, size_t key_len) {
    if (!table || !key) return PPDB_ERROR;
    if (table->is_immutable) return PPDB_ERROR;

    ppdb_sync_lock(&table->sync);

    void* value;
    size_t value_len;
    int ret = ppdb_skiplist_find(table->skiplist, key, key_len, &value, &value_len);
    if (ret == PPDB_OK) {
        free(value);
        ret = ppdb_skiplist_remove(table->skiplist, key, key_len);
        if (ret == PPDB_OK) {
            table->stats.size--;
            table->stats.memory_usage -= key_len + value_len;
            if (table->enable_compression) {
                table->stats.compressed_size -= value_len;
            }
        }
    }

    ppdb_sync_unlock(&table->sync);
    return ret;
}

// 转换为不可变表
void ppdb_memtable_make_immutable(ppdb_memtable_t* table) {
    if (!table) return;
    ppdb_sync_lock(&table->sync);
    table->is_immutable = true;
    ppdb_sync_unlock(&table->sync);
}

// 检查是否为不可变表
bool ppdb_memtable_is_immutable(ppdb_memtable_t* table) {
    if (!table) return false;
    return table->is_immutable;
}

// 获取内存使用量
size_t ppdb_memtable_memory_usage(ppdb_memtable_t* table) {
    if (!table) return 0;
    return table->stats.memory_usage;
}

// 获取数据条数
size_t ppdb_memtable_size(ppdb_memtable_t* table) {
    if (!table) return 0;
    return table->stats.size;
}
