#ifndef PPDB_KVSTORE_H
#define PPDB_KVSTORE_H

#include <cosmopolitan.h>
#include "error.h"
#include "defs.h"

// KVStore结构
typedef struct ppdb_kvstore_t {
    char db_path[MAX_PATH_LENGTH];  // 数据库路径
    struct ppdb_memtable_t* table;  // 内存表
    struct ppdb_wal_t* wal;         // WAL日志
    pthread_mutex_t mutex;          // 并发控制
} ppdb_kvstore_t;

// 打开KVStore
ppdb_error_t ppdb_kvstore_open(const char* path, ppdb_kvstore_t** store);

// 关闭KVStore
void ppdb_kvstore_close(ppdb_kvstore_t* store);

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