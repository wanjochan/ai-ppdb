#ifndef POLY_DB_H
#define POLY_DB_H

#include "internal/infra/infra_core.h"

// 数据库类型
typedef enum {
    POLY_DB_TYPE_SQLITE,
    POLY_DB_TYPE_DUCKDB
} poly_db_type_t;

// 基础类型定义
typedef void* poly_db_handle_t;      // 通用数据库句柄
typedef void* poly_db_stmt_t;        // 通用语句句柄
typedef void* poly_db_connection_t;  // 通用连接句柄
typedef void* poly_db_result_t;      // 通用结果集句柄

// 数据库句柄
struct poly_db;
typedef struct poly_db poly_db_t;

// // 数据库迭代器 TODO
// struct poly_db_iter;
// typedef struct poly_db_iter poly_db_iter_t;

// 统一的数据库接口
typedef struct poly_db_interface {
    // 连接管理
    infra_error_t (*open)(poly_db_t** db, const char* url);
    void (*close)(poly_db_t* db);
    
    // // KV 操作
    // infra_error_t (*get)(poly_db_t* db, const char* key, size_t key_len, void** value, size_t* value_size);
    // infra_error_t (*set)(poly_db_t* db, const char* key, size_t key_len, const void* value, size_t value_size);
    // infra_error_t (*del)(poly_db_t* db, const char* key, size_t key_len);
    
    // // 迭代器
    // infra_error_t (*iter_create)(poly_db_t* db, poly_db_iter_t** iter);
    // infra_error_t (*iter_next)(poly_db_iter_t* iter, char** key, void** value, size_t* value_size);
    // void (*iter_destroy)(poly_db_iter_t* iter);
    
    // SQL 执行
    infra_error_t (*exec)(poly_db_t* db, const char* sql);
} poly_db_interface_t;

// 主要接口函数
infra_error_t poly_db_open(const char* url, poly_db_t** db);
void poly_db_close(poly_db_t* db);

infra_error_t poly_db_exec(poly_db_t* db, const char* sql);

#endif // POLY_DB_H
