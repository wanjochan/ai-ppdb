#ifndef PPDB_METRICS_H
#define PPDB_METRICS_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"

// 性能指标操作函数声明

// 初始化性能指标
void ppdb_metrics_init(ppdb_metrics_t* metrics);

// 重置性能指标
void ppdb_metrics_reset(ppdb_metrics_t* metrics);

// 记录操作延迟
void ppdb_metrics_record_latency(ppdb_metrics_t* metrics, uint64_t latency_us);

// 记录写入操作
void ppdb_metrics_record_put(ppdb_metrics_t* metrics, size_t key_size, size_t value_size);

// 记录读取操作
void ppdb_metrics_record_get(ppdb_metrics_t* metrics, size_t key_size, size_t value_size);

// 记录删除操作
void ppdb_metrics_record_delete(ppdb_metrics_t* metrics, size_t key_size);

// 记录一般操作
void ppdb_metrics_record_op(ppdb_metrics_t* metrics, uint64_t latency_us);

// 获取总操作数
uint64_t ppdb_metrics_total_ops(ppdb_metrics_t* metrics);

// 获取平均延迟
uint64_t ppdb_metrics_avg_latency(ppdb_metrics_t* metrics);

// 获取总字节数
uint64_t ppdb_metrics_total_bytes(ppdb_metrics_t* metrics);

#endif // PPDB_METRICS_H
