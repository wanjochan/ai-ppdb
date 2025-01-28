#ifndef POLY_DUCKDBKV_H
#define POLY_DUCKDBKV_H

#include "internal/infra/infra_core.h"
#include "duckdb.h"
#include "internal/poly/poly_plugin.h"

// DuckDB KV 数据库句柄
struct poly_duckdbkv_db;
typedef struct poly_duckdbkv_db poly_duckdbkv_db_t;

// DuckDB KV 迭代器
struct poly_duckdbkv_iter;
typedef struct poly_duckdbkv_iter poly_duckdbkv_iter_t;

// DuckDB KV 接口
typedef struct poly_duckdbkv_interface {
    // 初始化
    infra_error_t (*init)(void** handle);
    
    // 清理
    void (*cleanup)(void* handle);
    
    // 基本操作
    infra_error_t (*open)(void* handle, const char* path);
    infra_error_t (*close)(void* handle);
    infra_error_t (*exec)(void* handle, const char* sql);
    
    // KV 操作
    infra_error_t (*get)(void* handle, const char* key, size_t key_len, void** value, size_t* value_size);
    infra_error_t (*set)(void* handle, const char* key, size_t key_len, const void* value, size_t value_size);
    infra_error_t (*del)(void* handle, const char* key, size_t key_len);
    
    // 迭代器
    infra_error_t (*iter_create)(void* handle, void** iter);
    infra_error_t (*iter_next)(void* iter, char** key, void** value, size_t* value_size);
    void (*iter_destroy)(void* iter);
} poly_duckdbkv_interface_t;

// 全局 DuckDB KV 接口实例
extern const poly_duckdbkv_interface_t g_duckdbkv_interface;

// 获取DuckDB KV插件接口
const poly_plugin_interface_t* poly_duckdbkv_get_interface(void);

// 打开和关闭数据库
infra_error_t poly_duckdbkv_open(poly_duckdbkv_db_t** db, const char* path);
void poly_duckdbkv_close(poly_duckdbkv_db_t* db);

// 基本操作
infra_error_t poly_duckdbkv_set(void* db, const char* key, size_t key_len, const void* value, size_t value_len);
infra_error_t poly_duckdbkv_get(void* db, const char* key, size_t key_len, void** value, size_t* value_len);
infra_error_t poly_duckdbkv_del(void* db, const char* key, size_t key_len);

// 事务操作
infra_error_t poly_duckdbkv_begin(poly_duckdbkv_db_t* db);
infra_error_t poly_duckdbkv_commit(poly_duckdbkv_db_t* db);
infra_error_t poly_duckdbkv_rollback(poly_duckdbkv_db_t* db);

// 迭代器操作
infra_error_t poly_duckdbkv_iter_create(poly_duckdbkv_db_t* db, poly_duckdbkv_iter_t** iter);
infra_error_t poly_duckdbkv_iter_next(poly_duckdbkv_iter_t* iter, char** key, size_t* key_len, void** value, size_t* value_len);
void poly_duckdbkv_iter_destroy(poly_duckdbkv_iter_t* iter);

typedef struct poly_duckdbkv_ctx {
    duckdb_database db;
    duckdb_connection conn;
    duckdb_prepared_statement get_stmt;
    duckdb_prepared_statement set_stmt;
    duckdb_prepared_statement del_stmt;
    duckdb_prepared_statement iter_stmt;
} poly_duckdbkv_ctx_t;

/**
 * @brief 执行 SQL 语句
 * @param ctx DuckDB KV 上下文
 * @param sql SQL 语句
 * @return 错误码
 */
infra_error_t poly_duckdbkv_exec(poly_duckdbkv_ctx_t* ctx, const char* sql);

#endif // POLY_DUCKDBKV_H 