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
    ppdb_node_state_machine_t state_machine;  // 节点状态机
    ppdb_sync_t* lock;                        // 节点锁
    ppdb_key_t* key;                         // 键
    ppdb_value_t* value;                     // 值
    ppdb_sync_counter_t height;              // 节点高度
    ppdb_sync_counter_t is_deleted;          // 删除标记
    ppdb_sync_counter_t is_garbage;          // 垃圾标记
    struct ppdb_node* next[];                // 后继节点数组
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

//-----------------------------------------------------------------------------
// 节点操作函数声明
//-----------------------------------------------------------------------------
ppdb_node_t* node_create(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value, uint32_t height);
void node_destroy(ppdb_node_t* node);
void node_ref(ppdb_node_t* node);
void node_unref(ppdb_node_t* node);
uint32_t node_get_height(ppdb_node_t* node);
ppdb_node_t* node_get_next(ppdb_node_t* node, uint32_t level);
void node_set_next(ppdb_node_t* node, uint32_t level, ppdb_node_t* next);
bool node_cas_next(ppdb_node_t* node, uint32_t level, ppdb_node_t* expected, ppdb_node_t* desired);
bool node_is_deleted(ppdb_node_t* node);
bool node_mark_deleted(ppdb_node_t* node);
bool node_is_active(ppdb_node_t* node);
bool node_try_mark(ppdb_node_t* node);

// 节点状态管理函数声明
void init_node_state_machine(ppdb_node_state_machine_t* sm);
bool try_enter_state(ppdb_node_state_machine_t* sm, ppdb_node_state_t expected, ppdb_node_state_t desired);
bool safe_ref_inc(ppdb_node_state_machine_t* sm);
bool safe_ref_dec(ppdb_node_state_machine_t* sm);
void wait_readers(ppdb_node_state_machine_t* sm);

#endif // PPDB_INTERNAL_H 
