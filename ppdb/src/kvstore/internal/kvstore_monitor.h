#ifndef PPDB_KVSTORE_MONITOR_H
#define PPDB_KVSTORE_MONITOR_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"

// 监控器结构
typedef struct ppdb_monitor ppdb_monitor_t;

// 创建监控器
ppdb_error_t ppdb_monitor_create(ppdb_monitor_t** monitor);

// 销毁监控器
void ppdb_monitor_destroy(ppdb_monitor_t* monitor);

// 操作监控
void ppdb_monitor_op_start(ppdb_monitor_t* monitor);
void ppdb_monitor_op_end(ppdb_monitor_t* monitor, uint64_t latency_us);

// 状态检查
bool ppdb_monitor_should_switch(ppdb_monitor_t* monitor);

// 指标获取
uint64_t ppdb_monitor_get_op_count(ppdb_monitor_t* monitor);
uint64_t ppdb_monitor_get_avg_latency(ppdb_monitor_t* monitor);
uint64_t ppdb_monitor_get_memory_usage(ppdb_monitor_t* monitor);

#endif // PPDB_KVSTORE_MONITOR_H 