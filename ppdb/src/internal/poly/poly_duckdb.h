#ifndef POLY_DUCKDB_H
#define POLY_DUCKDB_H

#include "internal/infra/infra_core.h"

// DuckDB 接口
typedef struct poly_duckdb_interface {
    // 初始化
    infra_error_t (*init)(void** handle);
    
    // 清理
    void (*cleanup)(void* handle);
    
    // 基本操作
    infra_error_t (*open)(void* handle, const char* path);
    infra_error_t (*close)(void* handle);
    infra_error_t (*exec)(void* handle, const char* sql);
    
    // KV 操作
    infra_error_t (*get)(void* handle, const char* key, void** value, size_t* value_size);
    infra_error_t (*set)(void* handle, const char* key, const void* value, size_t value_size);
    infra_error_t (*del)(void* handle, const char* key);
    
    // 迭代器
    infra_error_t (*iter_create)(void* handle, void** iter);
    infra_error_t (*iter_next)(void* iter, char** key, void** value, size_t* value_size);
    void (*iter_destroy)(void* iter);
} poly_duckdb_interface_t;

// 全局 DuckDB 接口实例
extern const poly_duckdb_interface_t g_duckdb_interface;

#endif // POLY_DUCKDB_H 