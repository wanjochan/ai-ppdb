#include "internal/poly/poly_duckdb.h"
#include "internal/infra/infra_core.h"
#include "duckdb.h"

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
    return INFRA_OK;
}

// 清理DuckDB模块
infra_error_t poly_duckdb_cleanup(void) {
    return INFRA_OK;
}

// 打开数据库
infra_error_t poly_duckdb_open(const char* path, poly_duckdb_db_t** db) {
    if (!path || !db) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_db_t* ddb = infra_malloc(sizeof(poly_duckdb_db_t));
    if (!ddb) return INFRA_ERROR_NO_MEMORY;
    
    if (duckdb_open(path, &ddb->db) != DuckDBSuccess) {
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    if (duckdb_connect(ddb->db, &ddb->conn) != DuckDBSuccess) {
        duckdb_close(&ddb->db);
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    // 创建KV表
    const char* create_sql = 
        "CREATE TABLE IF NOT EXISTS kv ("
        "  key BLOB PRIMARY KEY,"
        "  value BLOB"
        ")";
    duckdb_state state = duckdb_query(ddb->conn, create_sql, NULL);
    if (state != DuckDBSuccess) {
        duckdb_disconnect(&ddb->conn);
        duckdb_close(&ddb->db);
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    // 准备语句
    const char* get_sql = "SELECT value FROM kv WHERE key = ?";
    if (duckdb_prepare(ddb->conn, get_sql, &ddb->get_stmt) != DuckDBSuccess) {
        duckdb_disconnect(&ddb->conn);
        duckdb_close(&ddb->db);
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    const char* put_sql = "INSERT OR REPLACE INTO kv (key, value) VALUES (?, ?)";
    if (duckdb_prepare(ddb->conn, put_sql, &ddb->put_stmt) != DuckDBSuccess) {
        duckdb_destroy_prepare(&ddb->get_stmt);
        duckdb_disconnect(&ddb->conn);
        duckdb_close(&ddb->db);
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    const char* del_sql = "DELETE FROM kv WHERE key = ?";
    if (duckdb_prepare(ddb->conn, del_sql, &ddb->del_stmt) != DuckDBSuccess) {
        duckdb_destroy_prepare(&ddb->get_stmt);
        duckdb_destroy_prepare(&ddb->put_stmt);
        duckdb_disconnect(&ddb->conn);
        duckdb_close(&ddb->db);
        infra_free(ddb);
        return INFRA_ERROR_IO;
    }
    
    *db = ddb;
    return INFRA_OK;
}

// 关闭数据库
infra_error_t poly_duckdb_close(poly_duckdb_db_t* db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    
    duckdb_destroy_prepare(&db->get_stmt);
    duckdb_destroy_prepare(&db->put_stmt);
    duckdb_destroy_prepare(&db->del_stmt);
    duckdb_disconnect(&db->conn);
    duckdb_close(&db->db);
    infra_free(db);
    
    return INFRA_OK;
}

// KV操作
infra_error_t poly_duckdb_get(poly_duckdb_db_t* db, const void* key, size_t klen, void** val, size_t* vlen) {
    if (!db || !key || !val || !vlen) return INFRA_ERROR_INVALID_PARAM;
    
    // 绑定参数
    duckdb_bind_blob(db->get_stmt, 1, key, klen);
    
    // 执行查询
    duckdb_result result;
    if (duckdb_execute_prepared(db->get_stmt, &result) != DuckDBSuccess) {
        return INFRA_ERROR_IO;
    }
    
    // 检查结果
    if (duckdb_value_is_null(&result, 0, 0)) {
        duckdb_destroy_result(&result);
        return INFRA_ERROR_NOT_FOUND;
    }
    
    // 获取BLOB数据
    size_t blob_size;
    void* blob_data = (void*)duckdb_value_blob(&result, 0, 0, &blob_size);
    
    // 复制数据
    void* data = infra_malloc(blob_size);
    if (!data) {
        duckdb_destroy_result(&result);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    memcpy(data, blob_data, blob_size);
    *val = data;
    *vlen = blob_size;
    
    duckdb_destroy_result(&result);
    return INFRA_OK;
}

infra_error_t poly_duckdb_put(poly_duckdb_db_t* db, const void* key, size_t klen, const void* val, size_t vlen) {
    if (!db || !key || !val) return INFRA_ERROR_INVALID_PARAM;
    
    // 绑定参数
    duckdb_bind_blob(db->put_stmt, 1, key, klen);
    duckdb_bind_blob(db->put_stmt, 2, val, vlen);
    
    // 执行语句
    duckdb_result result;
    duckdb_state state = duckdb_execute_prepared(db->put_stmt, &result);
    duckdb_destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t poly_duckdb_del(poly_duckdb_db_t* db, const void* key, size_t klen) {
    if (!db || !key) return INFRA_ERROR_INVALID_PARAM;
    
    // 绑定参数
    duckdb_bind_blob(db->del_stmt, 1, key, klen);
    
    // 执行语句
    duckdb_result result;
    duckdb_state state = duckdb_execute_prepared(db->del_stmt, &result);
    duckdb_destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

// 事务操作
infra_error_t poly_duckdb_begin(poly_duckdb_db_t* db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    
    duckdb_result result;
    duckdb_state state = duckdb_query(db->conn, "BEGIN TRANSACTION", &result);
    duckdb_destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t poly_duckdb_commit(poly_duckdb_db_t* db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    
    duckdb_result result;
    duckdb_state state = duckdb_query(db->conn, "COMMIT", &result);
    duckdb_destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t poly_duckdb_rollback(poly_duckdb_db_t* db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    
    duckdb_result result;
    duckdb_state state = duckdb_query(db->conn, "ROLLBACK", &result);
    duckdb_destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

// 迭代器
infra_error_t poly_duckdb_iter_create(poly_duckdb_db_t* db, poly_duckdb_iter_t** iter) {
    if (!db || !iter) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_iter_t* it = infra_malloc(sizeof(poly_duckdb_iter_t));
    if (!it) return INFRA_ERROR_NO_MEMORY;
    
    const char* sql = "SELECT key, value FROM kv";
    if (duckdb_prepare(db->conn, sql, &it->stmt) != DuckDBSuccess) {
        infra_free(it);
        return INFRA_ERROR_IO;
    }
    
    if (duckdb_execute_prepared(it->stmt, &it->result) != DuckDBSuccess) {
        duckdb_destroy_prepare(&it->stmt);
        infra_free(it);
        return INFRA_ERROR_IO;
    }
    
    it->db = db;
    it->current_row = 0;
    it->total_rows = duckdb_row_count(&it->result);
    
    *iter = it;
    return INFRA_OK;
}

infra_error_t poly_duckdb_iter_next(poly_duckdb_iter_t* iter, void** key, size_t* klen, void** val, size_t* vlen) {
    if (!iter || !key || !klen || !val || !vlen) return INFRA_ERROR_INVALID_PARAM;
    
    if (iter->current_row >= iter->total_rows) {
        return INFRA_ERROR_NOT_FOUND;
    }
    
    // 获取key
    size_t key_size;
    void* key_data = (void*)duckdb_value_blob(&iter->result, 0, iter->current_row, &key_size);
    void* key_copy = infra_malloc(key_size);
    if (!key_copy) return INFRA_ERROR_NO_MEMORY;
    memcpy(key_copy, key_data, key_size);
    *key = key_copy;
    *klen = key_size;
    
    // 获取value
    size_t val_size;
    void* val_data = (void*)duckdb_value_blob(&iter->result, 1, iter->current_row, &val_size);
    void* val_copy = infra_malloc(val_size);
    if (!val_copy) {
        infra_free(key_copy);
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(val_copy, val_data, val_size);
    *val = val_copy;
    *vlen = val_size;
    
    iter->current_row++;
    return INFRA_OK;
}

infra_error_t poly_duckdb_iter_destroy(poly_duckdb_iter_t* iter) {
    if (!iter) return INFRA_ERROR_INVALID_PARAM;
    
    duckdb_destroy_result(&iter->result);
    duckdb_destroy_prepare(&iter->stmt);
    infra_free(iter);
    
    return INFRA_OK;
} 