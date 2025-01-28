#include "internal/infra/infra_core.h"
#include "internal/poly/poly_duckdbkv.h"
#include "internal/poly/poly_db.h"
#include "duckdb.h"

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
struct poly_duckdbkv_db {
    poly_db_t* db;  // 使用 poly_db 接口
};

// DuckDB KV 迭代器
struct poly_duckdbkv_iter {
    duckdb_result result;
    size_t current_row;
    size_t total_rows;
};

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
    if (!db || !path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 分配新的数据库句柄
    poly_duckdbkv_db_t* ddb = (poly_duckdbkv_db_t*)infra_malloc(sizeof(poly_duckdbkv_db_t));
    if (!ddb) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(ddb, 0, sizeof(poly_duckdbkv_db_t));

    // 构造 DuckDB URL
    char url[1024];
    snprintf(url, sizeof(url), "duckdb://%s", path);

    // 使用 poly_db 打开数据库
    infra_error_t err = poly_db_open(url, &ddb->db);
    if (err != INFRA_OK) {
        infra_free(ddb);
        return err;
    }

    // 创建表
    err = poly_db_exec(ddb->db, 
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "key VARCHAR PRIMARY KEY,"
        "value BLOB"
        ");"
    );
    if (err != INFRA_OK) {
        poly_db_close(ddb->db);
        infra_free(ddb);
        return err;
    }

    *db = ddb;
    return INFRA_OK;
}

// 关闭数据库
void poly_duckdbkv_close(poly_duckdbkv_db_t* db) {
    if (!db) {
        return;
    }

    if (db->db) {
        poly_db_close(db->db);
        db->db = NULL;
    }
    infra_free(db);
}

// 设置键值对
//TODO 这里可以优化成 upsert 而且不需要用到事务。。。

infra_error_t poly_duckdbkv_set(void* handle, const char* key, size_t key_len,
                             const void* value, size_t value_len) {
    if (!handle || !key || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdbkv_db_t* db = (poly_duckdbkv_db_t*)handle;
    if (!db->db) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 开始事务
    infra_error_t err = poly_db_exec(db->db, "BEGIN TRANSACTION");
    if (err != INFRA_OK) {
        return err;
    }

    // 先删除已存在的键
    char del_sql[1024];
    snprintf(del_sql, sizeof(del_sql), 
        "DELETE FROM kv_store WHERE key = '%.*s'", 
        (int)key_len, key);
    
    err = poly_db_exec(db->db, del_sql);
    if (err != INFRA_OK) {
        poly_db_exec(db->db, "ROLLBACK");
        return err;
    }

    // 插入新的键值对
    char ins_sql[1024];
    snprintf(ins_sql, sizeof(ins_sql),
        "INSERT INTO kv_store (key, value) VALUES ('%.*s', ?)",
        (int)key_len, key);
    
    err = poly_db_exec(db->db, ins_sql);
    if (err != INFRA_OK) {
        poly_db_exec(db->db, "ROLLBACK");
        return err;
    }

    // 提交事务
    return poly_db_exec(db->db, "COMMIT");
}

// 获取键值对
infra_error_t poly_duckdbkv_get(void* handle, const char* key, size_t key_len,
                             void** value, size_t* value_len) {
    if (!handle || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdbkv_db_t* db = (poly_duckdbkv_db_t*)handle;
    if (!db->db) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 构造查询 SQL
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "SELECT value FROM kv_store WHERE key = '%.*s'",
        (int)key_len, key);

    // 执行查询
    poly_db_result_t result;
    infra_error_t err = poly_db_query(db->db, sql, &result);
    if (err != INFRA_OK) {
        return err;
    }

    // 检查结果
    size_t row_count;
    err = poly_db_result_row_count(result, &row_count);
    if (err != INFRA_OK) {
        poly_db_result_free(result);
        return err;
    }

    if (row_count == 0) {
        poly_db_result_free(result);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 获取 BLOB 数据
    err = poly_db_result_get_blob(result, 0, 0, value, value_len);
    poly_db_result_free(result);
    return err;
}

// 删除键值对
infra_error_t poly_duckdbkv_del(void* handle, const char* key, size_t key_len) {
    if (!handle || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdbkv_db_t* db = (poly_duckdbkv_db_t*)handle;
    if (!db->db) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 构造删除 SQL
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "DELETE FROM kv_store WHERE key = '%.*s'",
        (int)key_len, key);

    // 执行删除
    return poly_db_exec(db->db, sql);
}

// 创建迭代器
infra_error_t poly_duckdbkv_iter_create(poly_duckdbkv_db_t* db, poly_duckdbkv_iter_t** iter) {
    if (!db || !iter) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdbkv_iter_t* iterator = infra_malloc(sizeof(poly_duckdbkv_iter_t));
    if (!iterator) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(iterator, 0, sizeof(poly_duckdbkv_iter_t));

    // 执行查询
    infra_error_t err = poly_db_query(db->db, "SELECT key, value FROM kv_store", &iterator->result);
    if (err != INFRA_OK) {
        infra_free(iterator);
        return err;
    }

    // 获取总行数
    err = poly_db_result_row_count(iterator->result, &iterator->total_rows);
    if (err != INFRA_OK) {
        poly_db_result_free(iterator->result);
        infra_free(iterator);
        return err;
    }

    iterator->current_row = 0;
    *iter = iterator;
    return INFRA_OK;
}

// 获取下一个键值对
infra_error_t poly_duckdbkv_iter_next(poly_duckdbkv_iter_t* iter, char** key, size_t* key_len, void** value, size_t* value_len) {
    if (!iter || !key || !key_len || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (iter->current_row >= iter->total_rows) {
        return INFRA_ERROR_NOT_FOUND;
    }

    // 获取键
    infra_error_t err = poly_db_result_get_string(iter->result, iter->current_row, 0, key);
    if (err != INFRA_OK) {
        return err;
    }
    *key_len = strlen(*key);

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
    if (!iter) {
        return;
    }

    poly_db_result_free(iter->result);
    infra_free(iter);
}

// 执行 SQL
infra_error_t poly_duckdbkv_exec(poly_duckdbkv_ctx_t* ctx, const char* sql) {
    if (!ctx || !sql) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // TODO: 实现 SQL 执行
    // 这部分需要等 poly_db 支持更多功能后再实现
    return INFRA_ERROR_NOT_IMPLEMENTED;
}

// 全局 DuckDB 接口实例
const poly_duckdbkv_interface_t g_duckdbkv_interface = {
    .init = poly_duckdbkv_init,
    .cleanup = poly_duckdbkv_cleanup,
    .open = poly_duckdbkv_open,
    .close = poly_duckdbkv_close,
    .get = poly_duckdbkv_get,
    .set = poly_duckdbkv_set,
    .del = poly_duckdbkv_del,
    .exec = wrap_exec,
    .iter_create = wrap_iter_create,
    .iter_next = wrap_iter_next,
    .iter_destroy = wrap_iter_destroy
};

// DuckDB 插件接口实例
static const poly_plugin_interface_t g_duckdb_plugin_interface = {
    .init = poly_duckdbkv_init,
    .cleanup = poly_duckdbkv_cleanup,
    .set = poly_duckdbkv_set,
    .get = poly_duckdbkv_get,
    .del = poly_duckdbkv_del
};

// 获取 DuckDB 插件接口
const poly_plugin_interface_t* poly_duckdbkv_get_interface(void) {
    return &g_duckdb_plugin_interface;
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

    infra_error_t err = poly_duckdbkv_get((poly_duckdbkv_db_t*)handle, temp_key, key_len, value, value_size);
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

    infra_error_t err = poly_duckdbkv_set((poly_duckdbkv_db_t*)handle, temp_key, key_len, value, value_size);
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

    infra_error_t err = poly_duckdbkv_del((poly_duckdbkv_db_t*)handle, temp_key, key_len);
    infra_free(temp_key);
    return err;
}



