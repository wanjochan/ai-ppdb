// KV存储基础头文件
#ifndef KVSTORE_H
#define KVSTORE_H

#include "cosmopolitan.h"

// KV存储状态码
typedef enum {
    KV_OK = 0,
    KV_ERR_NOT_FOUND = -1,
    KV_ERR_NO_MEMORY = -2,
    KV_ERR_INVALID = -3,
    KV_ERR_IO = -4
} kv_status;

// KV存储句柄
typedef struct KVStore KVStore;

// 创建KV存储实例
KVStore* kv_open(const char* path);

// 关闭KV存储实例
void kv_close(KVStore* store);

// 基础操作接口
kv_status kv_put(KVStore* store, const uint8_t* key, size_t key_len, 
                 const uint8_t* value, size_t value_len);
kv_status kv_get(KVStore* store, const uint8_t* key, size_t key_len,
                 uint8_t* value, size_t* value_len);
kv_status kv_delete(KVStore* store, const uint8_t* key, size_t key_len);

// 迭代器接口
typedef struct KVIterator KVIterator;

KVIterator* kv_iterator_new(KVStore* store);
void kv_iterator_free(KVIterator* iter);
int kv_iterator_valid(const KVIterator* iter);
void kv_iterator_next(KVIterator* iter);
const uint8_t* kv_iterator_key(const KVIterator* iter, size_t* key_len);
const uint8_t* kv_iterator_value(const KVIterator* iter, size_t* value_len);

#endif // KVSTORE_H 