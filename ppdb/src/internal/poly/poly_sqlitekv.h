#ifndef POLY_SQLITEKV_H
#define POLY_SQLITEKV_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/poly/poly_db.h"

// SQLite KV 数据库句柄
typedef struct poly_sqlitekv_db {
    poly_db_t* db;  // 使用 poly_db 接口
} poly_sqlitekv_db_t;

// SQLite KV 迭代器
typedef struct poly_sqlitekv_iter {
    poly_sqlitekv_db_t* db;
    poly_db_result_t* result;
    size_t current_row;
    size_t total_rows;
} poly_sqlitekv_iter_t;

// Interface functions
infra_error_t poly_sqlitekv_open(poly_sqlitekv_db_t** db, const char* path);
void poly_sqlitekv_close(poly_sqlitekv_db_t* db);
infra_error_t poly_sqlitekv_get(poly_sqlitekv_db_t* db, const char* key, void** value, size_t* value_len);
infra_error_t poly_sqlitekv_set(poly_sqlitekv_db_t* db, const char* key, const void* value, size_t value_len);
infra_error_t poly_sqlitekv_del(poly_sqlitekv_db_t* db, const char* key);
infra_error_t poly_sqlitekv_exec(poly_sqlitekv_db_t* db, const char* sql);

// Iterator functions
infra_error_t poly_sqlitekv_iter_create(poly_sqlitekv_db_t* db, poly_sqlitekv_iter_t** iter);
infra_error_t poly_sqlitekv_iter_next(poly_sqlitekv_iter_t* iter, char** key, void** value, size_t* value_len);
void poly_sqlitekv_iter_destroy(poly_sqlitekv_iter_t* iter);

#endif // POLY_SQLITEKV_H 