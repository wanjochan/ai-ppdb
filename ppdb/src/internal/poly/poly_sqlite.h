#ifndef POLY_SQLITE_H
#define POLY_SQLITE_H

#include "internal/infra/infra_core.h"

// SQLite数据库句柄
typedef struct poly_sqlite_db poly_sqlite_db_t;

// 初始化SQLite模块
infra_error_t poly_sqlite_init(void);

// 清理SQLite模块
infra_error_t poly_sqlite_cleanup(void);

// 打开数据库
infra_error_t poly_sqlite_open(const char* path, poly_sqlite_db_t** db);

// 关闭数据库
infra_error_t poly_sqlite_close(poly_sqlite_db_t* db);

// KV操作
infra_error_t poly_sqlite_get(poly_sqlite_db_t* db, const void* key, size_t klen, void** val, size_t* vlen);
infra_error_t poly_sqlite_put(poly_sqlite_db_t* db, const void* key, size_t klen, const void* val, size_t vlen);
infra_error_t poly_sqlite_del(poly_sqlite_db_t* db, const void* key, size_t klen);

// 事务操作
infra_error_t poly_sqlite_begin(poly_sqlite_db_t* db);
infra_error_t poly_sqlite_commit(poly_sqlite_db_t* db);
infra_error_t poly_sqlite_rollback(poly_sqlite_db_t* db);

// 迭代器
typedef struct poly_sqlite_iter poly_sqlite_iter_t;

infra_error_t poly_sqlite_iter_create(poly_sqlite_db_t* db, poly_sqlite_iter_t** iter);
infra_error_t poly_sqlite_iter_next(poly_sqlite_iter_t* iter, void** key, size_t* klen, void** val, size_t* vlen);
infra_error_t poly_sqlite_iter_destroy(poly_sqlite_iter_t* iter);

#endif // POLY_SQLITE_H 