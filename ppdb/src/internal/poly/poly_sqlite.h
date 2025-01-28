#ifndef POLY_SQLITE_H
#define POLY_SQLITE_H

#include "internal/infra/infra_core.h"
#include "internal/poly/poly_plugin.h"

// SQLite 数据库句柄
typedef struct poly_sqlite_db poly_sqlite_db_t;

// SQLite 迭代器句柄
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
    infra_error_t (*get)(void* handle, const char* key, void** value, size_t* value_size);
    infra_error_t (*set)(void* handle, const char* key, const void* value, size_t value_size);
    infra_error_t (*del)(void* handle, const char* key);
    
    // 迭代器
    infra_error_t (*iter_create)(void* handle, void** iter);
    infra_error_t (*iter_next)(void* iter, char** key, void** value, size_t* value_size);
    void (*iter_destroy)(void* iter);
} poly_sqlite_interface_t;

// 全局 SQLite 接口实例
extern const poly_sqlite_interface_t g_sqlite_interface;

// 获取SQLite插件接口
const poly_plugin_interface_t* poly_sqlite_get_interface(void);

// 基本操作
infra_error_t poly_sqlite_open(const char* path, poly_sqlite_db_t** db);
infra_error_t poly_sqlite_close(poly_sqlite_db_t* db);
infra_error_t poly_sqlite_exec(poly_sqlite_db_t* db, const char* sql);

// KV 操作
infra_error_t poly_sqlite_get(poly_sqlite_db_t* db, const void* key, size_t klen, void** val, size_t* vlen);
infra_error_t poly_sqlite_put(poly_sqlite_db_t* db, const void* key, size_t klen, const void* val, size_t vlen);
infra_error_t poly_sqlite_del(poly_sqlite_db_t* db, const void* key, size_t klen);

// 事务操作
infra_error_t poly_sqlite_begin(poly_sqlite_db_t* db);
infra_error_t poly_sqlite_commit(poly_sqlite_db_t* db);
infra_error_t poly_sqlite_rollback(poly_sqlite_db_t* db);

// 迭代器操作
infra_error_t poly_sqlite_iter_create(poly_sqlite_db_t* db, poly_sqlite_iter_t** iter);
infra_error_t poly_sqlite_iter_next(poly_sqlite_iter_t* iter, void** key, size_t* klen, void** val, size_t* vlen);
infra_error_t poly_sqlite_iter_destroy(poly_sqlite_iter_t* iter);

#endif // POLY_SQLITE_H 