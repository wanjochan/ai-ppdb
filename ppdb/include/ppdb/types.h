#ifndef PPDB_TYPES_H
#define PPDB_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "defs.h"
#include "error.h"

// 基础数据类型定义
typedef struct {
    uint8_t* data;
    size_t size;
} ppdb_slice_t;

// 配置相关结构
typedef struct {
    size_t cache_size;        // 缓存大小
    bool sync_write;          // 同步写入
    uint32_t max_file_size;   // 最大文件大小
    uint32_t block_size;      // 块大小
} ppdb_config_t;

// 迭代器接口
typedef struct ppdb_iterator ppdb_iterator_t;
struct ppdb_iterator {
    void* internal;
    bool (*valid)(const ppdb_iterator_t*);
    void (*next)(ppdb_iterator_t*);
    ppdb_slice_t (*key)(const ppdb_iterator_t*);
    ppdb_slice_t (*value)(const ppdb_iterator_t*);
    void (*destroy)(ppdb_iterator_t*);
};

// 统计信息结构
typedef struct {
    size_t mem_usage;         // 内存使用量
    size_t total_keys;        // 键总数
    uint64_t read_ops;        // 读操作次数
    uint64_t write_ops;       // 写操作次数
    uint64_t delete_ops;      // 删除操作次数
} ppdb_stats_t;

#endif // PPDB_TYPES_H 