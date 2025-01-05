#ifndef PPDB_INTERNAL_H
#define PPDB_INTERNAL_H

#include "ppdb/ppdb.h"

// 基础定义
typedef struct ppdb_sync ppdb_sync_t;
typedef struct ppdb_metrics ppdb_metrics_t;

// 统计信息结构定义
typedef struct ppdb_stats {
    uint64_t node_count;
    uint64_t key_count;
    uint64_t memory_usage;
    uint64_t get_ops;
    uint64_t put_ops;
    uint64_t remove_ops;
} PPDB_ALIGNED ppdb_stats_t;

// 分片结构定义
typedef struct ppdb_shard {
    ppdb_node_t* head;
    ppdb_sync_t* lock;
    ppdb_metrics_t metrics;
} PPDB_ALIGNED ppdb_shard_t;

// 常量定义
#define PPDB_MAX_HEIGHT MAX_SKIPLIST_LEVEL
#define PPDB_MAX_SHARDS DEFAULT_SHARD_COUNT
#define PPDB_LEVEL_PROBABILITY 25  // 25% 的概率增加层级

// 错误码定义
#define PPDB_ERR_NO_MEMORY PPDB_ERR_OUT_OF_MEMORY
#define PPDB_ERR_INVALID_CONFIG PPDB_ERR_INVALID_STATE

// 基础函数声明
uint64_t ppdb_random(void);
uint32_t random_level(void);
ppdb_error_t validate_and_setup_config(ppdb_config_t* config);
ppdb_error_t init_metrics(ppdb_metrics_t* metrics);

// 存储层函数声明
void cleanup_base(ppdb_base_t* base);
ppdb_error_t ppdb_key_copy(ppdb_key_t* dst, const ppdb_key_t* src);
ppdb_error_t ppdb_value_copy(ppdb_value_t* dst, const ppdb_value_t* src);
void ppdb_key_cleanup(ppdb_key_t* key);
void ppdb_value_cleanup(ppdb_value_t* value);

#endif // PPDB_INTERNAL_H 
