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

// 随机数生成器定义
typedef struct ppdb_random_state {
    uint64_t seed[4];    // xoshiro256** 算法需要的状态
} PPDB_ALIGNED ppdb_random_state_t;

// 常量定义
#define PPDB_MAX_HEIGHT MAX_SKIPLIST_LEVEL
#define PPDB_MAX_SHARDS DEFAULT_SHARD_COUNT
#define PPDB_LEVEL_PROBABILITY 0.25  // 修正为0.25，表示25%的概率

// 错误码定义
#define PPDB_ERR_NO_MEMORY PPDB_ERR_OUT_OF_MEMORY
#define PPDB_ERR_INVALID_CONFIG PPDB_ERR_INVALID_STATE

// 基础函数声明
void init_random(void);
uint32_t random_level(void);
ppdb_error_t validate_and_setup_config(ppdb_config_t* config);
ppdb_error_t init_metrics(ppdb_metrics_t* metrics);

// 随机数生成器函数声明
void ppdb_random_init(ppdb_random_state_t* state, uint64_t seed);
uint64_t ppdb_random_next(ppdb_random_state_t* state);
double ppdb_random_double(ppdb_random_state_t* state);  // 返回 [0, 1) 范围的双精度浮点数

// 节点操作函数声明
ppdb_node_t* node_create(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value, uint32_t height);
void node_destroy(ppdb_node_t* node);
uint32_t node_get_height(ppdb_node_t* node);
void node_ref(ppdb_node_t* node);
void node_unref(ppdb_node_t* node);
ppdb_node_t* node_get_next(ppdb_node_t* node, uint32_t level);
void node_set_next(ppdb_node_t* node, uint32_t level, ppdb_node_t* next);
bool node_cas_next(ppdb_node_t* node, uint32_t level, ppdb_node_t* expected, ppdb_node_t* desired);
bool node_is_deleted(ppdb_node_t* node);
bool node_mark_deleted(ppdb_node_t* node);

// 分片操作函数声明
ppdb_shard_t* get_shard(ppdb_base_t* base, const ppdb_key_t* key);
ppdb_error_t aggregate_shard_stats(ppdb_base_t* base, ppdb_stats_t* stats);

// 存储层函数声明
void cleanup_base(ppdb_base_t* base);
ppdb_error_t ppdb_key_copy(ppdb_key_t* dst, const ppdb_key_t* src);
ppdb_error_t ppdb_value_copy(ppdb_value_t* dst, const ppdb_value_t* src);
void ppdb_key_cleanup(ppdb_key_t* key);
void ppdb_value_cleanup(ppdb_value_t* value);

#endif // PPDB_INTERNAL_H 
