#ifndef PPDB_WAL_H
#define PPDB_WAL_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"

// WAL 配置
typedef struct ppdb_wal_config {
    char dir_path[PPDB_MAX_PATH_SIZE];  // WAL 目录路径
    char filename[PPDB_MAX_PATH_SIZE];  // WAL 文件名
    size_t segment_size;                // 段大小
    size_t max_segments;                // 最大段数量
    size_t max_total_size;             // 最大总大小
    size_t max_records;                // 最大记录数量
    bool sync_write;                   // 是否同步写入
    bool use_buffer;                   // 是否使用缓冲区
    size_t buffer_size;                // 缓冲区大小
    ppdb_compression_t compression;     // 压缩算法
    bool enable_compression;           // 启用压缩选项
} ppdb_wal_config_t;

// WAL 统计信息
typedef struct ppdb_wal_stats {
    size_t total_segments;       // 总段数
    size_t sealed_segments;      // 已封存段数
    size_t total_size;          // 总大小
    size_t active_size;         // 活跃数据大小
} ppdb_wal_stats_t;

// WAL 段信息
typedef struct ppdb_wal_segment_info {
    uint64_t id;                // 段 ID
    size_t size;               // 段大小
    bool is_sealed;            // 是否已封存
    uint64_t first_sequence;   // 第一个记录的序列号
    uint64_t last_sequence;    // 最后一个记录的序列号
} ppdb_wal_segment_info_t;

// WAL 恢复点信息
typedef struct ppdb_wal_recovery_point {
    uint64_t min_sequence;     // 最小序列号
    uint64_t max_sequence;     // 最大序列号
    size_t total_segments;     // 总段数
} ppdb_wal_recovery_point_t;

// 写入操作
typedef struct ppdb_write_op {
    const void* key;           // 键
    size_t key_size;          // 键大小
    const void* value;        // 值
    size_t value_size;       // 值大小
} ppdb_write_op_t;

// 写入批次
typedef struct ppdb_write_batch {
    ppdb_write_op_t* ops;     // 操作数组
    size_t count;            // 操作数量
} ppdb_write_batch_t;

// WAL 类型前向声明
typedef struct ppdb_wal ppdb_wal_t;

// WAL 基本操作
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, const void* key, size_t key_size,
                           const void* value, size_t value_size);
ppdb_error_t ppdb_wal_write_batch(ppdb_wal_t* wal, const ppdb_write_batch_t* batch);
ppdb_error_t ppdb_wal_sync(ppdb_wal_t* wal);
size_t ppdb_wal_size(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence(ppdb_wal_t* wal);

// WAL 维护操作
ppdb_error_t ppdb_wal_cleanup(ppdb_wal_t* wal, uint64_t min_sequence);
ppdb_error_t ppdb_wal_compact(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_stats(ppdb_wal_t* wal, ppdb_wal_stats_t* stats);
ppdb_error_t ppdb_wal_get_segment_info(ppdb_wal_t* wal, size_t index,
                                     ppdb_wal_segment_info_t* info);
ppdb_error_t ppdb_wal_get_recovery_point(ppdb_wal_t* wal,
                                       ppdb_wal_recovery_point_t* recovery_point);

// WAL 迭代器类型前向声明
typedef struct ppdb_wal_iterator ppdb_wal_iterator_t;

// WAL 迭代器操作
ppdb_error_t ppdb_wal_iterator_create(ppdb_wal_t* wal, ppdb_wal_iterator_t** iterator);
void ppdb_wal_iterator_destroy(ppdb_wal_iterator_t* iterator);
bool ppdb_wal_iterator_valid(ppdb_wal_iterator_t* iterator);
ppdb_error_t ppdb_wal_iterator_next(ppdb_wal_iterator_t* iterator);
ppdb_error_t ppdb_wal_iterator_get(ppdb_wal_iterator_t* iterator,
                                 void** key, size_t* key_size,
                                 void** value, size_t* value_size);
uint64_t ppdb_wal_iterator_sequence(ppdb_wal_iterator_t* iterator);
ppdb_error_t ppdb_wal_iterator_reset(ppdb_wal_iterator_t* iterator);
ppdb_error_t ppdb_wal_iterator_seek(ppdb_wal_iterator_t* iterator, uint64_t sequence);

#endif // PPDB_WAL_H 