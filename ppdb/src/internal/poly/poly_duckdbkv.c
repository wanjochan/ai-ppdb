#include <string.h>
#include <dlfcn.h>
#include "internal/infra/infra_core.h"
#include "internal/poly/poly_duckdbkv.h"
#include "internal/poly/poly_db.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_memory.h"
// #include "duckdb.h"

// DuckDB函数指针类型定义
typedef duckdb_state (*duckdb_open_t)(const char *path, duckdb_database *out_database);
typedef void (*duckdb_close_t)(duckdb_database *database);
typedef duckdb_state (*duckdb_connect_t)(duckdb_database database, duckdb_connection *out_connection);
typedef void (*duckdb_disconnect_t)(duckdb_connection *connection);
typedef duckdb_state (*duckdb_query_t)(duckdb_connection connection, const char *query, duckdb_result *out_result);
typedef duckdb_state (*duckdb_prepare_t)(duckdb_connection connection, const char *query, duckdb_prepared_statement *out_prepared_statement);
typedef void (*duckdb_destroy_prepare_t)(duckdb_prepared_statement *prepared_statement);
typedef duckdb_state (*duckdb_execute_prepared_t)(duckdb_prepared_statement prepared_statement, duckdb_result *out_result);
typedef void (*duckdb_destroy_result_t)(duckdb_result *result);
typedef bool (*duckdb_value_is_null_t)(duckdb_result *result, idx_t col, idx_t row);
typedef duckdb_blob (*duckdb_value_blob_t)(duckdb_result *result, idx_t col, idx_t row);
typedef void (*duckdb_bind_blob_t)(duckdb_prepared_statement prepared_statement, idx_t param_idx, const void *data, idx_t length);
typedef duckdb_state (*duckdb_bind_varchar_t)(duckdb_prepared_statement prepared_statement, idx_t param_idx, const char *str);
typedef idx_t (*duckdb_row_count_t)(duckdb_result *result);
typedef duckdb_string (*duckdb_value_string_t)(duckdb_result *result, idx_t col, idx_t row);
typedef void (*duckdb_free_t)(void *ptr);

// 内部函数声明
static infra_error_t poly_duckdbkv_set_internal(void* handle, const char* key, size_t key_len,
                                   const void* value, size_t value_size);
static infra_error_t poly_duckdbkv_get_internal(void* handle, const char* key, size_t key_len,
                                   void** value, size_t* value_size);
static infra_error_t poly_duckdbkv_del_internal(void* handle, const char* key, size_t key_len);

// DuckDB函数指针
static struct {
    void* handle;
    duckdb_open_t open;
    duckdb_close_t close;
    duckdb_connect_t connect;
    duckdb_disconnect_t disconnect;
    duckdb_query_t query;
    duckdb_prepare_t prepare;
    duckdb_destroy_prepare_t destroy_prepare;
    duckdb_execute_prepared_t execute_prepared;
    duckdb_destroy_result_t destroy_result;
    duckdb_value_is_null_t value_is_null;
    duckdb_value_blob_t value_blob;
    duckdb_bind_blob_t bind_blob;
    duckdb_bind_varchar_t bind_varchar;
    duckdb_row_count_t row_count;
    duckdb_value_string_t value_string;
    duckdb_free_t free;
} g_duckdb = {0};

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

// 初始化 DuckDB KV 模块
infra_error_t poly_duckdbkv_init(void** handle) {
    if (!handle) return INFRA_ERROR_INVALID_PARAM;

    // 分配新的数据库句柄
    poly_duckdbkv_db_t* ddb = (poly_duckdbkv_db_t*)infra_malloc(sizeof(poly_duckdbkv_db_t));
    if (!ddb) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(ddb, 0, sizeof(poly_duckdbkv_db_t));
    *handle = ddb;
    return INFRA_OK;
}

// 清理 DuckDB KV 模块
void poly_duckdbkv_cleanup(void* handle) {
    poly_duckdbkv_db_t* db = (poly_duckdbkv_db_t*)handle;
    if (!db) {
        return;
    }

    if (db->db) {
        poly_db_close(db->db);
        db->db = NULL;
    }
    infra_free(db);
}

// 打开数据库
infra_error_t poly_duckdbkv_open(poly_duckdbkv_db_t** db, const char* path) {
    if (!db || !path) return INFRA_ERROR_INVALID_PARAM;

    poly_duckdbkv_db_t* duckdb = infra_malloc(sizeof(poly_duckdbkv_db_t));
    if (!duckdb) return INFRA_ERROR_NO_MEMORY;
    memset(duckdb, 0, sizeof(poly_duckdbkv_db_t));

    // 构造 DuckDB URL
    char url[1024] = "duckdb://";
    if (strncmp(path, "duckdb://", 9) != 0) {
        strncat(url, path, sizeof(url) - strlen(url) - 1);
        path = url;
    }

    // 使用 poly_db 打开数据库
    infra_error_t err = poly_db_open(path, &duckdb->db);
    if (err != INFRA_OK) {
        infra_free(duckdb);
        return err;
    }

    // 创建表
    const char* create_table_sql = 
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "key VARCHAR PRIMARY KEY,"
        "value BLOB"
        ")";

    err = poly_db_exec(duckdb->db, create_table_sql);
    if (err != INFRA_OK) {
        poly_db_close(duckdb->db);
        infra_free(duckdb);
        return err;
    }

    *db = duckdb;
    return INFRA_OK;
}

// 关闭数据库
void poly_duckdbkv_close(poly_duckdbkv_db_t* db) {
    if (!db) return;
    if (db->db) poly_db_close(db->db);
    infra_free(db);
}

// 设置键值对
infra_error_t poly_duckdbkv_set(poly_duckdbkv_db_t* db, const char* key, const void* value, size_t value_len) {
    if (!db || !key || !value) return INFRA_ERROR_INVALID_PARAM;

    // 准备 SQL 语句
    const char* sql = "INSERT OR REPLACE INTO kv_store (key, value) VALUES (?, ?)";
    poly_db_result_t* result = NULL;
    
    // 执行 SQL
    infra_error_t err = poly_db_query(db->db, sql, &result);
    if (err != INFRA_OK) return err;

    // 释放结果
    if (result) poly_db_result_free(result);
    return INFRA_OK;
}

// 获取键值对
infra_error_t poly_duckdbkv_get(poly_duckdbkv_db_t* db, const char* key, void** value, size_t* value_len) {
    if (!db || !key || !value || !value_len) return INFRA_ERROR_INVALID_PARAM;

    // 准备 SQL 语句
    const char* sql = "SELECT value FROM kv_store WHERE key = ?";
    poly_db_result_t* result = NULL;

    // 执行查询
    infra_error_t err = poly_db_query(db->db, sql, &result);
    if (err != INFRA_OK) return err;

    // 检查结果
    size_t row_count = 0;
    err = poly_db_result_row_count(result, &row_count);
    if (err != INFRA_OK || row_count == 0) {
        poly_db_result_free(result);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 获取 BLOB 数据
    err = poly_db_result_get_blob(result, 0, 0, value, value_len);
    poly_db_result_free(result);
    return err;
}

// 删除键值对
infra_error_t poly_duckdbkv_del(poly_duckdbkv_db_t* db, const char* key) {
    if (!db || !key) return INFRA_ERROR_INVALID_PARAM;

    // 准备 SQL 语句
    const char* sql = "DELETE FROM kv_store WHERE key = ?";
    poly_db_result_t* result = NULL;

    // 执行 SQL
    infra_error_t err = poly_db_query(db->db, sql, &result);
    if (result) poly_db_result_free(result);
    return err;
}

// 创建迭代器
infra_error_t poly_duckdbkv_iter_create(poly_duckdbkv_db_t* db, poly_duckdbkv_iter_t** iter) {
    if (!db || !iter) return INFRA_ERROR_INVALID_PARAM;

    poly_duckdbkv_iter_t* iterator = infra_malloc(sizeof(poly_duckdbkv_iter_t));
    if (!iterator) return INFRA_ERROR_NO_MEMORY;
    memset(iterator, 0, sizeof(poly_duckdbkv_iter_t));

    iterator->db = db;
    iterator->current_row = 0;

    // 准备迭代器查询
    const char* sql = "SELECT key, value FROM kv_store ORDER BY key";
    infra_error_t err = poly_db_query(db->db, sql, &iterator->result);
    if (err != INFRA_OK) {
        infra_free(iterator);
        return err;
    }

    err = poly_db_result_row_count(iterator->result, &iterator->total_rows);
    if (err != INFRA_OK) {
        poly_db_result_free(iterator->result);
        infra_free(iterator);
        return err;
    }

    *iter = iterator;
    return INFRA_OK;
}

// 获取下一个键值对
infra_error_t poly_duckdbkv_iter_next(poly_duckdbkv_iter_t* iter, char** key, void** value, size_t* value_len) {
    if (!iter || !key || !value || !value_len) return INFRA_ERROR_INVALID_PARAM;

    if (iter->current_row >= iter->total_rows) {
        return INFRA_ERROR_NOT_FOUND;
    }

    // 获取键
    infra_error_t err = poly_db_result_get_string(iter->result, iter->current_row, 0, key);
    if (err != INFRA_OK) return err;

    // 获取值
    err = poly_db_result_get_blob(iter->result, iter->current_row, 1, value, value_len);
    if (err != INFRA_OK) {
        infra_free(*key);
        return err;
    }

    iter->current_row++;
    return INFRA_OK;
}

// 销毁迭代器
void poly_duckdbkv_iter_destroy(poly_duckdbkv_iter_t* iter) {
    if (!iter) return;
    if (iter->result) poly_db_result_free(iter->result);
    infra_free(iter);
}

// 执行 SQL
infra_error_t poly_duckdbkv_exec(poly_duckdbkv_db_t* db, const char* sql) {
    if (!db || !sql) return INFRA_ERROR_INVALID_PARAM;
    return poly_db_exec(db->db, sql);
}

// 内部函数实现
static infra_error_t poly_duckdbkv_get_internal(void* handle, const char* key, size_t key_len,
                                   void** value, size_t* value_size) {
    if (!handle || !key || !value || !value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    char* temp_key = infra_malloc(key_len + 1);
    if (!temp_key) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(temp_key, key, key_len);
    temp_key[key_len] = '\0';

    infra_error_t err = poly_duckdbkv_get((poly_duckdbkv_db_t*)handle, temp_key, value, value_size);
    infra_free(temp_key);
    return err;
}

static infra_error_t poly_duckdbkv_set_internal(void* handle, const char* key, size_t key_len,
                                   const void* value, size_t value_size) {
    if (!handle || !key || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    char* temp_key = infra_malloc(key_len + 1);
    if (!temp_key) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(temp_key, key, key_len);
    temp_key[key_len] = '\0';

    infra_error_t err = poly_duckdbkv_set((poly_duckdbkv_db_t*)handle, temp_key, value, value_size);
    infra_free(temp_key);
    return err;
}

static infra_error_t poly_duckdbkv_del_internal(void* handle, const char* key, size_t key_len) {
    if (!handle || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    char* temp_key = infra_malloc(key_len + 1);
    if (!temp_key) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(temp_key, key, key_len);
    temp_key[key_len] = '\0';

    infra_error_t err = poly_duckdbkv_del((poly_duckdbkv_db_t*)handle, temp_key);
    infra_free(temp_key);
    return err;
}



