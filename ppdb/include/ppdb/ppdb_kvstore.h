#ifndef PPDB_KVSTORE_H
#define PPDB_KVSTORE_H

#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"

// KVStore操作函数
ppdb_error_t ppdb_kvstore_create(ppdb_kvstore_t** kvstore, ppdb_kvstore_config_t* config);
ppdb_error_t ppdb_kvstore_destroy(ppdb_kvstore_t* kvstore);
ppdb_error_t ppdb_kvstore_put(ppdb_kvstore_t* kvstore, void* key, size_t key_len, void* value, size_t value_len);
ppdb_error_t ppdb_kvstore_get(ppdb_kvstore_t* kvstore, void* key, size_t key_len, void* value, size_t* value_len);
ppdb_error_t ppdb_kvstore_delete(ppdb_kvstore_t* kvstore, void* key, size_t key_len);

#endif // PPDB_KVSTORE_H