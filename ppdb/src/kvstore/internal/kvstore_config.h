#ifndef PPDB_KVSTORE_CONFIG_H
#define PPDB_KVSTORE_CONFIG_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_kvstore.h"

// 内部配置扩展
typedef struct ppdb_kvstore_internal_config {
    size_t l0_size;               // L0文件大小限制
    size_t l0_files;              // L0文件数量限制
    bool enable_metrics;          // 是否启用性能指标
    bool enable_monitoring;       // 是否启用监控
    ppdb_compression_t compression; // 压缩算法
    ppdb_mode_t mode;             // 运行模式
} ppdb_kvstore_internal_config_t;

#endif // PPDB_KVSTORE_CONFIG_H 