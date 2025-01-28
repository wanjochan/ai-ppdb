#ifndef POLY_SQLITE_H
#define POLY_SQLITE_H

#include "internal/infra/infra_core.h"
#include "sqlite3.h"
#include "internal/poly/poly_plugin.h"

// SQLite 数据库句柄
struct poly_sqlite_db;
typedef struct poly_sqlite_db poly_sqlite_db_t;

// SQLite 迭代器
struct poly_sqlite_iter;
typedef struct poly_sqlite_iter poly_sqlite_iter_t;

// SQLite 接口
typedef struct poly_sqlite_interface {
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
} poly_sqlite_interface_t;

// 全局 SQLite 接口实例
extern const poly_sqlite_interface_t g_sqlite_interface;

// 获取SQLite插件接口
const poly_plugin_interface_t* poly_sqlite_get_interface(void);

// 打开和关闭数据库
infra_error_t poly_sqlite_open(poly_sqlite_db_t** db, const char* path);
void poly_sqlite_close(poly_sqlite_db_t* db);

// 基本操作
infra_error_t poly_sqlite_set(void* db, const char* key, size_t key_len, const void* value, size_t value_len);
infra_error_t poly_sqlite_get(void* db, const char* key, size_t key_len, void** value, size_t* value_len);
infra_error_t poly_sqlite_del(void* db, const char* key, size_t key_len);

// 事务操作
infra_error_t poly_sqlite_begin(poly_sqlite_db_t* db);
infra_error_t poly_sqlite_commit(poly_sqlite_db_t* db);
infra_error_t poly_sqlite_rollback(poly_sqlite_db_t* db);

// 迭代器操作
infra_error_t poly_sqlite_iter_create(poly_sqlite_db_t* db, poly_sqlite_iter_t** iter);
infra_error_t poly_sqlite_iter_next(poly_sqlite_iter_t* iter, char** key, size_t* key_len, void** value, size_t* value_len);
void poly_sqlite_iter_destroy(poly_sqlite_iter_t* iter);

typedef struct poly_sqlite_ctx {
    sqlite3* db;
    sqlite3_stmt* get_stmt;
    sqlite3_stmt* set_stmt;
    sqlite3_stmt* del_stmt;
    sqlite3_stmt* iter_stmt;
} poly_sqlite_ctx_t;

/**
 * @brief 执行 SQL 语句
 * @param ctx SQLite 上下文
 * @param sql SQL 语句
 * @return 错误码
 */
infra_error_t poly_sqlite_exec(poly_sqlite_ctx_t* ctx, const char* sql);

#endif // POLY_SQLITE_H 