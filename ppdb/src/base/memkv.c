#include "ppdb/ppdb_memkv.h"
#include "ppdb/base.h"
#include "ppdb/storage.h"

// 内部状态
typedef struct {
    ppdb_memkv_config_t config;
    ppdb_memkv_status_t status;
    ppdb_base_t* base;
} ppdb_memkv_t;

// 创建实例
ppdb_status_t ppdb_memkv_create(ppdb_base_t** base, const ppdb_memkv_config_t* config) {
    if (!base || !config) {
        return PPDB_INVALID_ARGUMENT;
    }

    // 分配内存
    ppdb_memkv_t* kv = calloc(1, sizeof(ppdb_memkv_t));
    if (!kv) {
        return PPDB_NO_MEMORY;
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

    ppdb_status_t status = ppdb_sharded_create(&kv->base, &storage_config);
    if (status != PPDB_OK) {
        free(kv);
        return status;
    }

    *base = kv->base;
    return PPDB_OK;
}

// ... 其余实现保持不变 ...
