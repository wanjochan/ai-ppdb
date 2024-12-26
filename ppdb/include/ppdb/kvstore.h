#ifndef PPDB_KVSTORE_H
#define PPDB_KVSTORE_H

#include "cosmopolitan.h"
#include "ppdb/error.h"

#ifdef __cplusplus
extern "C" {
#endif

// KVStore句柄
typedef struct ppdb_kvstore_t ppdb_kvstore_t;

// 创建/打开KVStore实例
ppdb_error_t ppdb_kvstore_open(const char* path, ppdb_kvstore_t** store);

// 关闭KVStore实例
void ppdb_kvstore_close(ppdb_kvstore_t* store);

// 基本操作
ppdb_error_t ppdb_kvstore_put(ppdb_kvstore_t* store, const uint8_t* key, size_t key_len,
                             const uint8_t* value, size_t value_len);
ppdb_error_t ppdb_kvstore_get(ppdb_kvstore_t* store, const uint8_t* key, size_t key_len,
                             uint8_t* value, size_t* value_len);
ppdb_error_t ppdb_kvstore_delete(ppdb_kvstore_t* store, const uint8_t* key, size_t key_len);

#ifdef __cplusplus
}
#endif

#endif // PPDB_KVSTORE_H 