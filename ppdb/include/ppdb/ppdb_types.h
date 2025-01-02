#ifndef PPDB_TYPES_H_
#define PPDB_TYPES_H_

#include <cosmopolitan.h>

// 最大路径长度
#define PPDB_MAX_PATH_SIZE 256

// 压缩类型
typedef enum {
    PPDB_COMPRESSION_NONE = 0,
    PPDB_COMPRESSION_SNAPPY,
    PPDB_COMPRESSION_LZ4,
    PPDB_COMPRESSION_ZSTD
} ppdb_compression_t;

// 键值对结构
typedef struct {
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;
} ppdb_kv_pair_t;

// 跳表结构
typedef struct ppdb_skiplist ppdb_skiplist_t;

// 迭代器结构
typedef struct ppdb_skiplist_iterator ppdb_skiplist_iterator_t;

#endif // PPDB_TYPES_H_ 