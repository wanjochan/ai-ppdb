#ifndef POLY_DUCKDB_H
#define POLY_DUCKDB_H

#include "internal/infra/infra_core.h"

// DuckDB数据库句柄
typedef struct poly_duckdb_db poly_duckdb_db_t;

// 初始化DuckDB模块
infra_error_t poly_duckdb_init(void);

// 清理DuckDB模块
infra_error_t poly_duckdb_cleanup(void);

// 打开数据库
infra_error_t poly_duckdb_open(const char* path, poly_duckdb_db_t** db);

// 关闭数据库
infra_error_t poly_duckdb_close(poly_duckdb_db_t* db);

// KV操作
infra_error_t poly_duckdb_get(poly_duckdb_db_t* db, const void* key, size_t klen, void** val, size_t* vlen);
infra_error_t poly_duckdb_put(poly_duckdb_db_t* db, const void* key, size_t klen, const void* val, size_t vlen);
infra_error_t poly_duckdb_del(poly_duckdb_db_t* db, const void* key, size_t klen);

// 事务操作
infra_error_t poly_duckdb_begin(poly_duckdb_db_t* db);
infra_error_t poly_duckdb_commit(poly_duckdb_db_t* db);
infra_error_t poly_duckdb_rollback(poly_duckdb_db_t* db);

// 迭代器
typedef struct poly_duckdb_iter poly_duckdb_iter_t;

infra_error_t poly_duckdb_iter_create(poly_duckdb_db_t* db, poly_duckdb_iter_t** iter);
infra_error_t poly_duckdb_iter_next(poly_duckdb_iter_t* iter, void** key, size_t* klen, void** val, size_t* vlen);
infra_error_t poly_duckdb_iter_destroy(poly_duckdb_iter_t* iter);

#endif // POLY_DUCKDB_H 