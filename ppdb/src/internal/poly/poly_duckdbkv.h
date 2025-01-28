#ifndef POLY_DUCKDBKV_H
#define POLY_DUCKDBKV_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/poly/poly_db.h"

// DuckDB KV 数据库句柄
typedef struct poly_duckdbkv_db {
    poly_db_t* db;  // 使用 poly_db 接口
} poly_duckdbkv_db_t;

// DuckDB KV 迭代器
typedef struct poly_duckdbkv_iter {
    poly_duckdbkv_db_t* db;
    poly_db_result_t* result;
    size_t current_row;
    size_t total_rows;
} poly_duckdbkv_iter_t;

// Interface functions
infra_error_t poly_duckdbkv_open(poly_duckdbkv_db_t** db, const char* path);
void poly_duckdbkv_close(poly_duckdbkv_db_t* db);
infra_error_t poly_duckdbkv_get(poly_duckdbkv_db_t* db, const char* key, void** value, size_t* value_len);
infra_error_t poly_duckdbkv_set(poly_duckdbkv_db_t* db, const char* key, const void* value, size_t value_len);
infra_error_t poly_duckdbkv_del(poly_duckdbkv_db_t* db, const char* key);
infra_error_t poly_duckdbkv_exec(poly_duckdbkv_db_t* db, const char* sql);

// Iterator functions
infra_error_t poly_duckdbkv_iter_create(poly_duckdbkv_db_t* db, poly_duckdbkv_iter_t** iter);
infra_error_t poly_duckdbkv_iter_next(poly_duckdbkv_iter_t* iter, char** key, void** value, size_t* value_len);
void poly_duckdbkv_iter_destroy(poly_duckdbkv_iter_t* iter);

#endif // POLY_DUCKDBKV_H 