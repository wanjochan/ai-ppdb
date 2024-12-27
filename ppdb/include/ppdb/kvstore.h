#ifndef PPDB_KVSTORE_H
#define PPDB_KVSTORE_H

#include <cosmopolitan.h>
#include "error.h"
#include "defs.h"

// 压缩算法
typedef enum {
    PPDB_COMPRESSION_NONE = 0,  // 不压缩
    PPDB_COMPRESSION_SNAPPY,    // Snappy压缩
    PPDB_COMPRESSION_LZ4        // LZ4压缩
} ppdb_compression_t;

// KVStore配置
typedef struct {
    char dir_path[MAX_PATH_LENGTH];  // 数据库目录路径
    size_t memtable_size;            // MemTable大小限制
    size_t l0_size;                  // L0文件大小限制
    size_t l0_files;                 // L0文件数量限制
    ppdb_compression_t compression;   // 压缩算法
} ppdb_kvstore_config_t;

// KVStore结构(不透明)
typedef struct ppdb_kvstore_t ppdb_kvstore_t;

// 创建KVStore
ppdb_error_t ppdb_kvstore_create(const ppdb_kvstore_config_t* config, ppdb_kvstore_t** store);

// 关闭KVStore
void ppdb_kvstore_close(ppdb_kvstore_t* store);

// 销毁KVStore及其所有数据
void ppdb_kvstore_destroy(ppdb_kvstore_t* store);

// 写入键值对
ppdb_error_t ppdb_kvstore_put(ppdb_kvstore_t* store,
                             const uint8_t* key, size_t key_len,
                             const uint8_t* value, size_t value_len);

// 读取键值对
ppdb_error_t ppdb_kvstore_get(ppdb_kvstore_t* store,
                             const uint8_t* key, size_t key_len,
                             uint8_t* value, size_t* value_len);

// 删除键值对
ppdb_error_t ppdb_kvstore_delete(ppdb_kvstore_t* store,
                                const uint8_t* key, size_t key_len);

#endif // PPDB_KVSTORE_H 