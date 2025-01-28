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
    duckdb_prepared_statement get_stmt;
    duckdb_prepared_statement put_stmt;
    duckdb_prepared_statement del_stmt;
};

// 迭代器结构
struct poly_duckdb_iter {
    poly_duckdb_db_t* db;
    duckdb_prepared_statement stmt;
    duckdb_result result;
    size_t current_row;
    size_t total_rows;
};

// 初始化DuckDB模块
infra_error_t poly_duckdb_init(void) {
    // 加载 DuckDB 动态库
    g_duckdb.handle = cosmo_dlopen("libduckdb.dylib", RTLD_LAZY);
    if (!g_duckdb.handle) {
        return INFRA_ERROR_IO;
    }

    // 获取函数指针
    g_duckdb.open = (duckdb_open_t)cosmo_dlsym(g_duckdb.handle, "duckdb_open");
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
    if (!g_duckdb.open || !g_duckdb.close || !g_duckdb.connect || !g_duckdb.disconnect ||
        !g_duckdb.query || !g_duckdb.prepare || !g_duckdb.destroy_prepare || !g_duckdb.execute_prepared ||
        !g_duckdb.destroy_result || !g_duckdb.value_is_null || !g_duckdb.value_blob ||
        !g_duckdb.bind_blob || !g_duckdb.row_count) {
        cosmo_dlclose(g_duckdb.handle);
        g_duckdb.handle = NULL;
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

// 清理DuckDB模块
infra_error_t poly_duckdb_cleanup(void) {
    if (g_duckdb.handle) {
        cosmo_dlclose(g_duckdb.handle);
        g_duckdb.handle = NULL;
    }
    return INFRA_OK;
}

// 打开数据库
infra_error_t poly_duckdb_open(const char* path, poly_duckdb_db_t** db) {
    if (!path || !db || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_db_t* ddb = infra_malloc(sizeof(poly_duckdb_db_t));
    if (!ddb) return INFRA_ERROR_NO_MEMORY;
    
    if (g_duckdb.open(path, &ddb->db) != DuckDBSuccess) {
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    if (g_duckdb.connect(ddb->db, &ddb->conn) != DuckDBSuccess) {
        g_duckdb.close(&ddb->db);
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    // 创建KV表
    const char* create_sql = 
        "CREATE TABLE IF NOT EXISTS kv ("
        "  key BLOB PRIMARY KEY,"
        "  value BLOB"
        ")";
    duckdb_result result;
    duckdb_state state = g_duckdb.query(ddb->conn, create_sql, &result);
    g_duckdb.destroy_result(&result);
    if (state != DuckDBSuccess) {
        g_duckdb.disconnect(&ddb->conn);
        g_duckdb.close(&ddb->db);
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    // 准备语句
    const char* get_sql = "SELECT value FROM kv WHERE key = ?";
    if (g_duckdb.prepare(ddb->conn, get_sql, &ddb->get_stmt) != DuckDBSuccess) {
        g_duckdb.disconnect(&ddb->conn);
        g_duckdb.close(&ddb->db);
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    const char* put_sql = "INSERT OR REPLACE INTO kv (key, value) VALUES (?, ?)";
    if (g_duckdb.prepare(ddb->conn, put_sql, &ddb->put_stmt) != DuckDBSuccess) {
        g_duckdb.destroy_prepare(&ddb->get_stmt);
        g_duckdb.disconnect(&ddb->conn);
        g_duckdb.close(&ddb->db);
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    const char* del_sql = "DELETE FROM kv WHERE key = ?";
    if (g_duckdb.prepare(ddb->conn, del_sql, &ddb->del_stmt) != DuckDBSuccess) {
        g_duckdb.destroy_prepare(&ddb->get_stmt);
        g_duckdb.destroy_prepare(&ddb->put_stmt);
        g_duckdb.disconnect(&ddb->conn);
        g_duckdb.close(&ddb->db);
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    *db = ddb;
    return INFRA_OK;
}

// 关闭数据库
infra_error_t poly_duckdb_close(poly_duckdb_db_t* db) {
    if (!db || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    g_duckdb.destroy_prepare(&db->get_stmt);
    g_duckdb.destroy_prepare(&db->put_stmt);
    g_duckdb.destroy_prepare(&db->del_stmt);
    g_duckdb.disconnect(&db->conn);
    g_duckdb.close(&db->db);
    infra_free(db);
    
    return INFRA_OK;
}

// KV操作
infra_error_t poly_duckdb_get(poly_duckdb_db_t* db, const void* key, size_t klen, void** val, size_t* vlen) {
    if (!db || !key || !val || !vlen || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    // 绑定参数
    g_duckdb.bind_blob(db->get_stmt, 1, key, klen);
    
    // 执行查询
    duckdb_result result;
    if (g_duckdb.execute_prepared(db->get_stmt, &result) != DuckDBSuccess) {
        return INFRA_ERROR_IO;
    }
    
    // 检查结果
    if (g_duckdb.value_is_null(&result, 0, 0)) {
        g_duckdb.destroy_result(&result);
        return INFRA_ERROR_NOT_FOUND;
    }
    
    // 获取BLOB数据
    duckdb_blob blob = g_duckdb.value_blob(&result, 0, 0);
    
    // 复制数据
    void* data = infra_malloc(blob.size);
    if (!data) {
        g_duckdb.destroy_result(&result);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    memcpy(data, blob.data, blob.size);
    *val = data;
    *vlen = blob.size;
    
    g_duckdb.destroy_result(&result);
    return INFRA_OK;
}

infra_error_t poly_duckdb_put(poly_duckdb_db_t* db, const void* key, size_t klen, const void* val, size_t vlen) {
    if (!db || !key || !val || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    // 绑定参数
    g_duckdb.bind_blob(db->put_stmt, 1, key, klen);
    g_duckdb.bind_blob(db->put_stmt, 2, val, vlen);
    
    // 执行语句
    duckdb_result result;
    duckdb_state state = g_duckdb.execute_prepared(db->put_stmt, &result);
    g_duckdb.destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t poly_duckdb_del(poly_duckdb_db_t* db, const void* key, size_t klen) {
    if (!db || !key || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    // 绑定参数
    g_duckdb.bind_blob(db->del_stmt, 1, key, klen);
    
    // 执行语句
    duckdb_result result;
    duckdb_state state = g_duckdb.execute_prepared(db->del_stmt, &result);
    g_duckdb.destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

// 事务操作
infra_error_t poly_duckdb_begin(poly_duckdb_db_t* db) {
    if (!db || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    duckdb_result result;
    duckdb_state state = g_duckdb.query(db->conn, "BEGIN TRANSACTION", &result);
    g_duckdb.destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t poly_duckdb_commit(poly_duckdb_db_t* db) {
    if (!db || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    duckdb_result result;
    duckdb_state state = g_duckdb.query(db->conn, "COMMIT", &result);
    g_duckdb.destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t poly_duckdb_rollback(poly_duckdb_db_t* db) {
    if (!db || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    duckdb_result result;
    duckdb_state state = g_duckdb.query(db->conn, "ROLLBACK", &result);
    g_duckdb.destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

// 迭代器
infra_error_t poly_duckdb_iter_create(poly_duckdb_db_t* db, poly_duckdb_iter_t** iter) {
    if (!db || !iter || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_iter_t* it = infra_malloc(sizeof(poly_duckdb_iter_t));
    if (!it) return INFRA_ERROR_NO_MEMORY;
    
    const char* sql = "SELECT key, value FROM kv";
    if (g_duckdb.prepare(db->conn, sql, &it->stmt) != DuckDBSuccess) {
        infra_free(it);
        return INFRA_ERROR_IO;
    }
    
    if (g_duckdb.execute_prepared(it->stmt, &it->result) != DuckDBSuccess) {
        g_duckdb.destroy_prepare(&it->stmt);
        infra_free(it);
        return INFRA_ERROR_IO;
    }
    
    it->db = db;
    it->current_row = 0;
    it->total_rows = g_duckdb.row_count(&it->result);
    
    *iter = it;
    return INFRA_OK;
}

infra_error_t poly_duckdb_iter_next(poly_duckdb_iter_t* iter, void** key, size_t* klen, void** val, size_t* vlen) {
    if (!iter || !key || !klen || !val || !vlen || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    if (iter->current_row >= iter->total_rows) {
        return INFRA_ERROR_NOT_FOUND;
    }
    
    // 获取key
    duckdb_blob key_blob = g_duckdb.value_blob(&iter->result, 0, iter->current_row);
    void* key_copy = infra_malloc(key_blob.size);
    if (!key_copy) return INFRA_ERROR_NO_MEMORY;
    memcpy(key_copy, key_blob.data, key_blob.size);
    *key = key_copy;
    *klen = key_blob.size;
    
    // 获取value
    duckdb_blob val_blob = g_duckdb.value_blob(&iter->result, 1, iter->current_row);
    void* val_copy = infra_malloc(val_blob.size);
    if (!val_copy) {
        infra_free(key_copy);
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(val_copy, val_blob.data, val_blob.size);
    *val = val_copy;
    *vlen = val_blob.size;
    
    iter->current_row++;
    return INFRA_OK;
}

infra_error_t poly_duckdb_iter_destroy(poly_duckdb_iter_t* iter) {
    if (!iter || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    g_duckdb.destroy_result(&iter->result);
    g_duckdb.destroy_prepare(&iter->stmt);
    infra_free(iter);
    
    return INFRA_OK;
}

// 全局接口实例
const poly_duckdb_interface_t g_duckdb_interface = {
    .init = poly_duckdb_init,
    .cleanup = poly_duckdb_cleanup,
    .open = poly_duckdb_open,
    .close = poly_duckdb_close,
    .exec = poly_duckdb_exec,
    .get = poly_duckdb_get,
    .set = poly_duckdb_put,
    .del = poly_duckdb_del,
    .iter_create = poly_duckdb_iter_create,
    .iter_next = poly_duckdb_iter_next,
    .iter_destroy = poly_duckdb_iter_destroy
}; 