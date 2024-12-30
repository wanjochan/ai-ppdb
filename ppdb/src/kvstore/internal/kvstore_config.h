#ifndef PPDB_KVSTORE_CONFIG_H
#define PPDB_KVSTORE_CONFIG_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "kvstore_types.h"

// KVStore configuration structure
typedef struct ppdb_kvstore_config {
    char db_path[256];              // 数据库路径
    size_t memtable_size;          // memtable大小限制
    size_t l0_size;               // L0文件大小限制
    size_t l0_files;              // L0文件数量限制
    ppdb_compression_t compression; // 压缩算法
    ppdb_mode_t mode;             // 运行模式
    bool adaptive_sharding;        // 是否启用自适应分片
} ppdb_kvstore_config_t;

#endif // PPDB_KVSTORE_CONFIG_H 