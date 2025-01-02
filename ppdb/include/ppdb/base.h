#ifndef PPDB_BASE_H
#define PPDB_BASE_H

#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/storage.h"

#ifdef __cplusplus
extern "C" {
#endif

// 前向声明
typedef struct ppdb_base ppdb_base_t;

// 基础操作结构
typedef struct ppdb_ops {
    ppdb_error_t (*get)(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
    ppdb_error_t (*put)(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
    ppdb_error_t (*remove)(ppdb_base_t* base, const ppdb_key_t* key);
    ppdb_error_t (*clear)(ppdb_base_t* base);
} ppdb_ops_t;

// 基础函数
ppdb_error_t ppdb_init(ppdb_base_t* base, const ppdb_storage_config_t* config);
ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key);
ppdb_error_t ppdb_clear(ppdb_base_t* base);
void ppdb_destroy(ppdb_base_t* base);

// 引用计数
void ppdb_ref(ppdb_base_t* base);
void ppdb_unref(ppdb_base_t* base);

// 类型获取
uint32_t ppdb_get_type(const ppdb_base_t* base);

#ifdef __cplusplus
}
#endif

#endif // PPDB_BASE_H
