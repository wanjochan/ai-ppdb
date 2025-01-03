#ifndef PPDB_ADVANCE_H
#define PPDB_ADVANCE_H

#include "ppdb/ppdb.h"

// 前向声明
struct ppdb_base;  // 避免循环引用

// 迭代器接口
typedef struct ppdb_iterator ppdb_iterator_t;
struct ppdb_iterator {
    void* internal;  // 内部实现数据
    ppdb_error_t (*next)(ppdb_iterator_t* iter);
    ppdb_error_t (*current)(ppdb_iterator_t* iter, ppdb_key_t* key, ppdb_value_t* value);
    void (*destroy)(ppdb_iterator_t* iter);
};

// 范围扫描选项
typedef struct ppdb_scan_options {
    ppdb_key_t* start_key;     // 起始键（NULL表示从头开始）
    ppdb_key_t* end_key;       // 结束键（NULL表示扫描到末尾）
    bool include_start;         // 是否包含起始键
    bool include_end;          // 是否包含结束键
    size_t batch_size;         // 批量获取大小（0表示使用默认值）
} ppdb_scan_options_t;

// 性能指标
typedef struct ppdb_metrics {
    // 基础计数器
    uint64_t get_count;        // Get操作总数
    uint64_t get_hits;         // Get操作命中数
    uint64_t put_count;        // Put操作总数
    uint64_t delete_count;     // Delete操作总数
    
    // 性能统计
    uint64_t avg_get_latency;  // 平均Get延迟（微秒）
    uint64_t avg_put_latency;  // 平均Put延迟（微秒）
    uint64_t scan_count;       // 范围扫描次数
    
    // 内存统计
    size_t memory_used;        // 当前内存使用量（字节）
    size_t memory_limit;       // 内存限制（字节）
} ppdb_metrics_t;

// 高级操作接口
typedef struct ppdb_advance_ops {
    // 范围扫描
    ppdb_error_t (*scan)(struct ppdb_base* base, 
                        const ppdb_scan_options_t* options,
                        ppdb_iterator_t** iterator);
    
    // 性能指标
    ppdb_error_t (*metrics_get)(struct ppdb_base* base,
                               ppdb_metrics_t* metrics);
} ppdb_advance_ops_t;

// 初始化高级功能
ppdb_error_t ppdb_advance_init(struct ppdb_base* base);

// 清理高级功能
void ppdb_advance_cleanup(struct ppdb_base* base);

#endif // PPDB_ADVANCE_H
