#ifndef POLY_SQLITEKV_H
#define POLY_SQLITEKV_H

#include "internal/infra/infra_core.h"
#include "sqlite3.h"
#include "internal/poly/poly_plugin.h"

// SQLite KV 数据库句柄
struct poly_sqlitekv_db;
typedef struct poly_sqlitekv_db poly_sqlitekv_db_t;

// SQLite KV 迭代器
struct poly_sqlitekv_iter;
typedef struct poly_sqlitekv_iter poly_sqlitekv_iter_t;

// SQLite KV 接口
typedef struct poly_sqlitekv_interface {
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
} poly_sqlitekv_interface_t;

// 全局 SQLite KV 接口实例
extern const poly_sqlitekv_interface_t g_sqlitekv_interface;

// 获取SQLite KV插件接口
const poly_plugin_interface_t* poly_sqlitekv_get_interface(void);

// 打开和关闭数据库
infra_error_t poly_sqlitekv_open(poly_sqlitekv_db_t** db, const char* path);
void poly_sqlitekv_close(poly_sqlitekv_db_t* db);

// 基本操作
infra_error_t poly_sqlitekv_set(void* db, const char* key, size_t key_len, const void* value, size_t value_len);
infra_error_t poly_sqlitekv_get(void* db, const char* key, size_t key_len, void** value, size_t* value_len);
infra_error_t poly_sqlitekv_del(void* db, const char* key, size_t key_len);

// 事务操作
infra_error_t poly_sqlitekv_begin(poly_sqlitekv_db_t* db);
infra_error_t poly_sqlitekv_commit(poly_sqlitekv_db_t* db);
infra_error_t poly_sqlitekv_rollback(poly_sqlitekv_db_t* db);

// 迭代器操作
infra_error_t poly_sqlitekv_iter_create(poly_sqlitekv_db_t* db, poly_sqlitekv_iter_t** iter);
infra_error_t poly_sqlitekv_iter_next(poly_sqlitekv_iter_t* iter, char** key, size_t* key_len, void** value, size_t* value_len);
void poly_sqlitekv_iter_destroy(poly_sqlitekv_iter_t* iter);

typedef struct poly_sqlitekv_ctx {
    sqlite3* db;
    sqlite3_stmt* get_stmt;
    sqlite3_stmt* set_stmt;
    sqlite3_stmt* del_stmt;
    sqlite3_stmt* iter_stmt;
} poly_sqlitekv_ctx_t;

/**
 * @brief 执行 SQL 语句
 * @param ctx SQLite KV 上下文
 * @param sql SQL 语句
 * @return 错误码
 */
infra_error_t poly_sqlitekv_exec(poly_sqlitekv_ctx_t* ctx, const char* sql);

#endif // POLY_SQLITEKV_H 