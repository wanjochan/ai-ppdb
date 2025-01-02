#include "ppdb/ppdb_memkv.h"
#include "ppdb/base.h"
#include "ppdb/storage.h"

// 内部状态
typedef struct {
    ppdb_memkv_config_t config;
    ppdb_memkv_status_t status;
    ppdb_base_t base;  // 改为直接存储而不是指针
} ppdb_memkv_t;

// 创建实例
ppdb_error_t ppdb_memkv_create(ppdb_base_t** base, const ppdb_memkv_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 分配内存
    ppdb_memkv_t* kv = calloc(1, sizeof(ppdb_memkv_t));
    if (!kv) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化配置
    kv->config = *config;
    
    // 创建存储实例
    ppdb_storage_config_t storage_config = {
        .initial_size = config->memory_limit / (config->shard_count ? config->shard_count : 16),
        .max_size = config->memory_limit,
        .flags = 0,
        .user_data = config->user_data
    };

    ppdb_error_t err = ppdb_sharded_create(&kv->base, &storage_config);
    if (err != PPDB_OK) {
        free(kv);
        return err;
    }

    // 初始化状态信息
    memset(&kv->status, 0, sizeof(ppdb_memkv_status_t));

    *base = &kv->base;
    return PPDB_OK;
}

void ppdb_memkv_destroy(ppdb_base_t* base) {
    if (!base) {
        return;
    }

    ppdb_memkv_t* kv = container_of(base, ppdb_memkv_t, base);
    ppdb_destroy(&kv->base);
    free(kv);
}

ppdb_error_t ppdb_memkv_get_status(ppdb_base_t* base, ppdb_memkv_status_t* status) {
    if (!base || !status) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_memkv_t* kv = container_of(base, ppdb_memkv_t, base);
    
    // 获取存储统计信息
    ppdb_storage_stats_t storage_stats;
    ppdb_error_t err = ppdb_storage_get_stats(&kv->base, &storage_stats);
    if (err != PPDB_OK) {
        return err;
    }

    // 更新状态信息
    status->memory_used = storage_stats.base_metrics.total_bytes;
    status->item_count = storage_stats.base_metrics.total_keys;
    status->stats = storage_stats.base_metrics;

    return PPDB_OK;
}
