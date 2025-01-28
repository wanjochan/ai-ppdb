#ifndef POLY_DUCKDB_H
#define POLY_DUCKDB_H

#include "internal/infra/infra_core.h"
#include "duckdb.h"
#include "internal/poly/poly_plugin.h"

// 成功状态码
#define DuckDB_SUCCESS DuckDBSuccess

// DuckDB 数据库句柄
struct poly_duckdb_db;
typedef struct poly_duckdb_db poly_duckdb_db_t;

// DuckDB 迭代器
struct poly_duckdb_iter;
typedef struct poly_duckdb_iter poly_duckdb_iter_t;

// DuckDB 接口
typedef struct poly_duckdb_interface {
    // 初始化
    infra_error_t (*init)(void** handle);
    
    // 清理
    void (*cleanup)(void* handle);
    
    // 基本操作
    infra_error_t (*open)(void* handle, const char* path);
    void (*close)(void* handle);
    infra_error_t (*exec)(void* handle, const char* sql);
    
    // KV 操作
    infra_error_t (*get)(void* handle, const char* key, size_t key_len, void** value, size_t* value_size);
    infra_error_t (*set)(void* handle, const char* key, size_t key_len, const void* value, size_t value_size);
    infra_error_t (*del)(void* handle, const char* key, size_t key_len);
    
    // 迭代器
    infra_error_t (*iter_create)(void* handle, void** iter);
    infra_error_t (*iter_next)(void* iter, char** key, void** value, size_t* value_size);
    void (*iter_destroy)(void* iter);
} poly_duckdb_interface_t;

// 全局 DuckDB 接口实例
extern const poly_duckdb_interface_t g_duckdb_interface;

// 获取 DuckDB 插件接口
const poly_plugin_interface_t* poly_duckdb_get_interface(void);

// 打开和关闭数据库
infra_error_t poly_duckdb_open(void* db, const char* path);
void poly_duckdb_close(void* db);

/**
 * @brief 设置键值对
 * @param db DuckDB 数据库实例
 * @param key 键
 * @param key_len 键长度
 * @param value 值
 * @param value_len 值长度
 * @return 错误码
 */
infra_error_t poly_duckdb_set(void* db, const char* key, size_t key_len,
                             const void* value, size_t value_len);

/**
 * @brief 获取键值对
 * @param db DuckDB 数据库实例
 * @param key 键
 * @param key_len 键长度
 * @param value 值的指针
 * @param value_len 值长度的指针
 * @return 错误码
 */
infra_error_t poly_duckdb_get(void* db, const char* key, size_t key_len,
                             void** value, size_t* value_len);

/**
 * @brief 删除键值对
 * @param db DuckDB 数据库实例
 * @param key 键
 * @param key_len 键长度
 * @return 错误码
 */
infra_error_t poly_duckdb_del(void* db, const char* key, size_t key_len);

// 迭代器操作
infra_error_t poly_duckdb_iter_create(void* db, void** iter);
infra_error_t poly_duckdb_iter_next(void* iter, char** key, void** value, size_t* value_len);
void poly_duckdb_iter_destroy(void* iter);

#endif // POLY_DUCKDB_H 