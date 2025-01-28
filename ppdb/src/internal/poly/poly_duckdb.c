#include "internal/poly/poly_duckdb.h"
#include "internal/infra/infra_core.h"
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
typedef idx_t (*duckdb_row_count_t)(duckdb_result *result);

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
    duckdb_row_count_t row_count;
} g_duckdb = {0};

// DuckDB数据库句柄
struct poly_duckdb_db {
    duckdb_database db;
    duckdb_connection conn;
    duckdb_prepared_statement stmt;
};

// DuckDB迭代器
struct poly_duckdb_iter {
    duckdb_result result;
    duckdb_connection conn;
    size_t current_row;
    size_t total_rows;
};

// 初始化DuckDB模块
infra_error_t poly_duckdb_init(void** handle) {
    fprintf(stderr, "DuckDB init starting...\n");
    if (!handle) return INFRA_ERROR_INVALID_PARAM;

    poly_duckdb_db_t* db = infra_malloc(sizeof(poly_duckdb_db_t));
    if (!db) {
        fprintf(stderr, "Failed to allocate memory for db handle\n");
        return INFRA_ERROR_NO_MEMORY;
    }

    // 加载 DuckDB 动态库
    const char* duckdb_path = getenv("DUCKDB_LIBRARY_PATH");
    fprintf(stderr, "DUCKDB_LIBRARY_PATH = %s\n", duckdb_path ? duckdb_path : "NULL");
    if (!duckdb_path) {
        fprintf(stderr, "DUCKDB_LIBRARY_PATH environment variable not set\n");
        infra_free(db);
        return INFRA_ERROR_IO;
    }
    fprintf(stderr, "Attempting to load DuckDB library from: %s\n", duckdb_path);
    g_duckdb.handle = cosmo_dlopen(duckdb_path, RTLD_LAZY);
    if (!g_duckdb.handle) {
        fprintf(stderr, "Failed to load DuckDB library from %s: %s\n", duckdb_path, cosmo_dlerror());
        infra_free(db);
        return INFRA_ERROR_IO;
    }
    fprintf(stderr, "Successfully loaded DuckDB library from %s\n", duckdb_path);

    // 获取函数指针
    g_duckdb.open = (duckdb_open_t)cosmo_dlsym(g_duckdb.handle, "duckdb_open");
    if (!g_duckdb.open) {
        fprintf(stderr, "Failed to get duckdb_open symbol: %s\n", cosmo_dlerror());
        cosmo_dlclose(g_duckdb.handle);
        g_duckdb.handle = NULL;
        infra_free(db);
        return INFRA_ERROR_IO;
    }

    g_duckdb.close = (duckdb_close_t)cosmo_dlsym(g_duckdb.handle, "duckdb_close");
    g_duckdb.connect = (duckdb_connect_t)cosmo_dlsym(g_duckdb.handle, "duckdb_connect");
    g_duckdb.disconnect = (duckdb_disconnect_t)cosmo_dlsym(g_duckdb.handle, "duckdb_disconnect");
    g_duckdb.query = (duckdb_query_t)cosmo_dlsym(g_duckdb.handle, "duckdb_query");
    g_duckdb.prepare = (duckdb_prepare_t)cosmo_dlsym(g_duckdb.handle, "duckdb_prepare");
    g_duckdb.destroy_prepare = (duckdb_destroy_prepare_t)cosmo_dlsym(g_duckdb.handle, "duckdb_destroy_prepare");
    g_duckdb.execute_prepared = (duckdb_execute_prepared_t)cosmo_dlsym(g_duckdb.handle, "duckdb_execute_prepared");
    g_duckdb.destroy_result = (duckdb_destroy_result_t)cosmo_dlsym(g_duckdb.handle, "duckdb_destroy_result");
    g_duckdb.value_is_null = (duckdb_value_is_null_t)cosmo_dlsym(g_duckdb.handle, "duckdb_value_is_null");
    g_duckdb.value_blob = (duckdb_value_blob_t)cosmo_dlsym(g_duckdb.handle, "duckdb_value_blob");
    g_duckdb.bind_blob = (duckdb_bind_blob_t)cosmo_dlsym(g_duckdb.handle, "duckdb_bind_blob");
    g_duckdb.row_count = (duckdb_row_count_t)cosmo_dlsym(g_duckdb.handle, "duckdb_row_count");

    // 验证所有函数指针
    if (!g_duckdb.close || !g_duckdb.connect || !g_duckdb.disconnect ||
        !g_duckdb.query || !g_duckdb.prepare || !g_duckdb.destroy_prepare || !g_duckdb.execute_prepared ||
        !g_duckdb.destroy_result || !g_duckdb.value_is_null || !g_duckdb.value_blob ||
        !g_duckdb.bind_blob || !g_duckdb.row_count) {
        fprintf(stderr, "Failed to get one or more DuckDB symbols\n");
        cosmo_dlclose(g_duckdb.handle);
        g_duckdb.handle = NULL;
        infra_free(db);
        return INFRA_ERROR_IO;
    }

    *handle = db;
    return INFRA_OK;
}

// 清理DuckDB模块
void poly_duckdb_cleanup(poly_duckdb_db_t* db) {
    if (!db) {
        return;
    }

    if (db->stmt) {
        g_duckdb.destroy_prepare(&db->stmt);
        db->stmt = NULL;
    }
    if (db->conn) {
        g_duckdb.disconnect(&db->conn);
        db->conn = NULL;
    }
}

// 打开数据库
infra_error_t poly_duckdb_open(void** db, const char* path) {
    if (!db || !path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 分配内存
    poly_duckdb_db_t* new_db = infra_malloc(sizeof(poly_duckdb_db_t));
    if (!new_db) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 打开数据库
    if (g_duckdb.open(path, &new_db->db) != DuckDB_SUCCESS) {
        infra_free(new_db);
        return INFRA_ERROR_IO;
    }

    // 创建连接
    if (g_duckdb.connect(new_db->db, &new_db->conn) != DuckDB_SUCCESS) {
        g_duckdb.close(&new_db->db);
        infra_free(new_db);
        return INFRA_ERROR_IO;
    }

    // 创建表
    duckdb_state state;
    state = g_duckdb.query(new_db->conn,
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "key BLOB PRIMARY KEY,"
        "value BLOB"
        ");",
        NULL);

    if (state != DuckDB_SUCCESS) {
        g_duckdb.disconnect(&new_db->conn);
        g_duckdb.close(&new_db->db);
        infra_free(new_db);
        return INFRA_ERROR_IO;
    }

    *db = new_db;
    return INFRA_OK;
}

// 关闭数据库
void poly_duckdb_close(void* db) {
    if (!db) {
        return;
    }

    poly_duckdb_db_t* ddb = (poly_duckdb_db_t*)db;
    g_duckdb.disconnect(&ddb->conn);
    g_duckdb.close(&ddb->db);
    infra_free(ddb);
}

// 执行 SQL 语句
infra_error_t poly_duckdb_exec(void* handle, const char* sql) {
    if (!handle || !sql || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    duckdb_result result;
    duckdb_state state = g_duckdb.query(db->conn, sql, &result);
    g_duckdb.destroy_result(&result);
    
    return (state == DuckDB_SUCCESS) ? INFRA_OK : INFRA_ERROR_IO;
}

// 设置键值对
infra_error_t poly_duckdb_set(void* db, const char* key,
                             const void* value, size_t value_len) {
    if (!db || !key || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* ddb = (poly_duckdb_db_t*)db;
    duckdb_prepared_statement stmt = NULL;
    duckdb_state state;
    duckdb_result result;

    state = g_duckdb.prepare(ddb->conn, "INSERT OR REPLACE INTO kv_store (key, value) VALUES (?, ?)", &stmt);
    if (state != DuckDB_SUCCESS) {
        return INFRA_ERROR_IO;
    }

    g_duckdb.bind_blob(stmt, 1, key, strlen(key));
    g_duckdb.bind_blob(stmt, 2, value, value_len);

    state = g_duckdb.execute_prepared(stmt, &result);
    g_duckdb.destroy_prepare(&stmt);
    g_duckdb.destroy_result(&result);

    return (state == DuckDB_SUCCESS) ? INFRA_OK : INFRA_ERROR_IO;
}

// 获取键值对
infra_error_t poly_duckdb_get(void* db, const char* key,
                             void** value, size_t* value_len) {
    if (!db || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* ddb = (poly_duckdb_db_t*)db;
    duckdb_prepared_statement stmt = NULL;
    duckdb_state state;
    duckdb_result result;

    state = g_duckdb.prepare(ddb->conn, "SELECT value FROM kv_store WHERE key = ?", &stmt);
    if (state != DuckDB_SUCCESS) {
        return INFRA_ERROR_IO;
    }

    g_duckdb.bind_blob(stmt, 1, key, strlen(key));
    state = g_duckdb.execute_prepared(stmt, &result);
    g_duckdb.destroy_prepare(&stmt);

    if (state != DuckDB_SUCCESS) {
        g_duckdb.destroy_result(&result);
        return INFRA_ERROR_IO;
    }

    if (g_duckdb.row_count(&result) == 0) {
        g_duckdb.destroy_result(&result);
        return INFRA_ERROR_NOT_FOUND;
    }

    duckdb_blob blob = g_duckdb.value_blob(&result, 0, 0);
    *value_len = blob.size;
    *value = infra_malloc(*value_len);
    if (!*value) {
        g_duckdb.destroy_result(&result);
        return INFRA_ERROR_NO_MEMORY;
    }

    memcpy(*value, blob.data, *value_len);
    g_duckdb.destroy_result(&result);
    return INFRA_OK;
}

// 删除键值对
infra_error_t poly_duckdb_del(void* db, const char* key) {
    if (!db || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* ddb = (poly_duckdb_db_t*)db;
    duckdb_prepared_statement stmt = NULL;
    duckdb_state state;
    duckdb_result result;

    state = g_duckdb.prepare(ddb->conn, "DELETE FROM kv_store WHERE key = ?", &stmt);
    if (state != DuckDB_SUCCESS) {
        return INFRA_ERROR_IO;
    }

    g_duckdb.bind_blob(stmt, 1, key, strlen(key));
    state = g_duckdb.execute_prepared(stmt, &result);
    g_duckdb.destroy_prepare(&stmt);
    g_duckdb.destroy_result(&result);

    return (state == DuckDB_SUCCESS) ? INFRA_OK : INFRA_ERROR_IO;
}

// 创建迭代器
infra_error_t poly_duckdb_iter_create(void* db, void** iter) {
    if (!db || !iter) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* ddb = (poly_duckdb_db_t*)db;
    poly_duckdb_iter_t* new_iter = infra_malloc(sizeof(poly_duckdb_iter_t));
    if (!new_iter) {
        return INFRA_ERROR_NO_MEMORY;
    }

    duckdb_result result;
    duckdb_state state = g_duckdb.query(ddb->conn, "SELECT key, value FROM kv_store", &result);
    if (state != DuckDB_SUCCESS) {
        infra_free(new_iter);
        return INFRA_ERROR_IO;
    }

    new_iter->result = result;
    new_iter->conn = ddb->conn;
    new_iter->current_row = 0;
    new_iter->total_rows = g_duckdb.row_count(&result);

    *iter = new_iter;
    return INFRA_OK;
}

// 迭代下一个键值对
infra_error_t poly_duckdb_iter_next(void* iter, char** key, void** value, size_t* value_len) {
    if (!iter || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_iter_t* it = (poly_duckdb_iter_t*)iter;
    if (it->current_row >= it->total_rows) {
        return INFRA_ERROR_NOT_FOUND;
    }

    duckdb_blob key_blob = g_duckdb.value_blob(&it->result, 0, it->current_row);
    duckdb_blob value_blob = g_duckdb.value_blob(&it->result, 1, it->current_row);

    *key = infra_malloc(key_blob.size + 1);
    if (!*key) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(*key, key_blob.data, key_blob.size);
    (*key)[key_blob.size] = '\0';

    *value_len = value_blob.size;
    *value = infra_malloc(*value_len);
    if (!*value) {
        infra_free(*key);
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(*value, value_blob.data, *value_len);

    it->current_row++;
    return INFRA_OK;
}

// 销毁迭代器
void poly_duckdb_iter_destroy(void* iter) {
    if (!iter) {
        return;
    }

    poly_duckdb_iter_t* it = (poly_duckdb_iter_t*)iter;
    g_duckdb.destroy_result(&it->result);
    infra_free(it);
}

// 全局 DuckDB 接口实例
const poly_duckdb_interface_t g_duckdb_interface = {
    .init = poly_duckdb_init,
    .cleanup = (void (*)(void*))poly_duckdb_cleanup,
    .open = (infra_error_t (*)(void*, const char*))poly_duckdb_open,
    .close = poly_duckdb_close,
    .exec = poly_duckdb_exec,
    .get = poly_duckdb_get,
    .set = poly_duckdb_set,
    .del = poly_duckdb_del,
    .iter_create = poly_duckdb_iter_create,
    .iter_next = poly_duckdb_iter_next,
    .iter_destroy = poly_duckdb_iter_destroy
}; 
