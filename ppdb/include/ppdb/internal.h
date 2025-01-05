#ifndef PPDB_INTERNAL_H
#define PPDB_INTERNAL_H

#include "ppdb/ppdb.h"

//-----------------------------------------------------------------------------
// 内部常量定义
//-----------------------------------------------------------------------------
#define PPDB_LEVEL_PROBABILITY 0.25  // 跳表层级概率
#define PPDB_MAX_LEVEL 32           // 跳表最大层数

//-----------------------------------------------------------------------------
// 内部数据结构
//-----------------------------------------------------------------------------
// 随机数生成器
typedef struct ppdb_random_state {
    uint64_t s[4];
} PPDB_ALIGNED ppdb_random_state_t;

// 度量指标
typedef struct ppdb_metrics {
    ppdb_sync_counter_t ops_count;      // 操作计数
    ppdb_sync_counter_t bytes_written;  // 写入字节数
    ppdb_sync_counter_t bytes_read;     // 读取字节数
    ppdb_sync_counter_t total_nodes;    // 总节点数
    ppdb_sync_counter_t total_keys;     // 总键数
    ppdb_sync_counter_t total_bytes;    // 总字节数
    ppdb_sync_counter_t total_gets;     // 总获取次数
    ppdb_sync_counter_t total_puts;     // 总写入次数
    ppdb_sync_counter_t total_removes;  // 总删除次数
} PPDB_ALIGNED ppdb_metrics_t;

// 统计信息
typedef struct ppdb_stats {
    size_t node_count;
    size_t key_count;
    size_t memory_usage;
    size_t get_ops;
    size_t put_ops;
    size_t remove_ops;
} PPDB_ALIGNED ppdb_stats_t;

// 节点结构
typedef struct ppdb_node {
    ppdb_sync_t* lock;              // lock for node
    ppdb_sync_counter_t height;     // height of node
    ppdb_sync_counter_t ref_count;  // reference count
    ppdb_key_t* key;               // key
    ppdb_value_t* value;           // value
    ppdb_sync_counter_t is_deleted; // deleted flag
    ppdb_sync_counter_t is_garbage; // garbage flag
    struct ppdb_node* next[];      // next pointers
} PPDB_ALIGNED ppdb_node_t;

// 分片结构
typedef struct ppdb_shard {
    ppdb_node_t* head;
    ppdb_sync_t* lock;
    ppdb_metrics_t metrics;
} PPDB_ALIGNED ppdb_shard_t;

// 基础结构
typedef struct ppdb_base {
    ppdb_config_t config;
    ppdb_shard_t* shards;
    ppdb_random_state_t random_state;
    ppdb_metrics_t metrics;
} PPDB_ALIGNED ppdb_base_t;

//-----------------------------------------------------------------------------
// 内部函数声明
//-----------------------------------------------------------------------------
// 随机数生成器操作
void ppdb_random_init(ppdb_random_state_t* state, uint64_t seed);
uint64_t ppdb_random_next(ppdb_random_state_t* state);
double ppdb_random_double(ppdb_random_state_t* state);
uint32_t random_level(void);

// 节点操作
ppdb_node_t* node_create(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value, uint32_t height);
void node_destroy(ppdb_node_t* node);
uint32_t node_get_height(ppdb_node_t* node);
void node_ref(ppdb_node_t* node);
void node_unref(ppdb_node_t* node);
bool node_is_deleted(ppdb_node_t* node);
bool node_mark_deleted(ppdb_node_t* node);

// 配置验证
ppdb_error_t validate_and_setup_config(ppdb_config_t* config);

// 内部函数声明
void init_random(void);
ppdb_error_t init_metrics(ppdb_metrics_t* metrics);
ppdb_shard_t* get_shard(ppdb_base_t* base, const ppdb_key_t* key);
ppdb_error_t aggregate_shard_stats(ppdb_base_t* base, ppdb_stats_t* stats);

#endif // PPDB_INTERNAL_H 
