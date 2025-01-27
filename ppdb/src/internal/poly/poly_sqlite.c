#include "internal/poly/poly_sqlite.h"
#include "internal/infra/infra_core.h"
#include "sqlite3.h"

// SQLite数据库句柄
struct poly_sqlite_db {
    sqlite3* db;
    sqlite3_stmt* get_stmt;
    sqlite3_stmt* put_stmt;
    sqlite3_stmt* del_stmt;
};

// 迭代器结构
struct poly_sqlite_iter {
    poly_sqlite_db_t* db;
    sqlite3_stmt* stmt;
};

// 初始化SQLite模块
infra_error_t poly_sqlite_init(void) {
    return INFRA_OK;
}

// 清理SQLite模块
infra_error_t poly_sqlite_cleanup(void) {
    return INFRA_OK;
}

// 打开数据库
infra_error_t poly_sqlite_open(const char* path, poly_sqlite_db_t** db) {
    if (!path || !db) return INFRA_ERROR_INVALID_PARAM;
    
    poly_sqlite_db_t* sdb = infra_malloc(sizeof(poly_sqlite_db_t));
    if (!sdb) return INFRA_ERROR_NO_MEMORY;
    
    int rc = sqlite3_open(path, &sdb->db);
    if (rc != SQLITE_OK) {
        infra_free(sdb);
        return INFRA_ERROR_IO;
    }
    
    // 创建KV表
    const char* create_sql = 
        "CREATE TABLE IF NOT EXISTS kv ("
        "  key BLOB PRIMARY KEY,"
        "  value BLOB"
        ")";
    char* err_msg = NULL;
    rc = sqlite3_exec(sdb->db, create_sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        sqlite3_close(sdb->db);
        infra_free(sdb);
        return INFRA_ERROR_IO;
    }
    
    // 准备语句
    const char* get_sql = "SELECT value FROM kv WHERE key = ?";
    rc = sqlite3_prepare_v2(sdb->db, get_sql, -1, &sdb->get_stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(sdb->db);
        infra_free(sdb);
        return INFRA_ERROR_IO;
    }
    
    const char* put_sql = "INSERT OR REPLACE INTO kv (key, value) VALUES (?, ?)";
    rc = sqlite3_prepare_v2(sdb->db, put_sql, -1, &sdb->put_stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(sdb->get_stmt);
        sqlite3_close(sdb->db);
        infra_free(sdb);
        return INFRA_ERROR_IO;
    }
    
    const char* del_sql = "DELETE FROM kv WHERE key = ?";
    rc = sqlite3_prepare_v2(sdb->db, del_sql, -1, &sdb->del_stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(sdb->get_stmt);
        sqlite3_finalize(sdb->put_stmt);
        sqlite3_close(sdb->db);
        infra_free(sdb);
        return INFRA_ERROR_IO;
    }
    
    *db = sdb;
    return INFRA_OK;
}

// 关闭数据库
infra_error_t poly_sqlite_close(poly_sqlite_db_t* db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    
    sqlite3_finalize(db->get_stmt);
    sqlite3_finalize(db->put_stmt);
    sqlite3_finalize(db->del_stmt);
    sqlite3_close(db->db);
    infra_free(db);
    
    return INFRA_OK;
}

// KV操作
infra_error_t poly_sqlite_get(poly_sqlite_db_t* db, const void* key, size_t klen, void** val, size_t* vlen) {
    if (!db || !key || !val || !vlen) return INFRA_ERROR_INVALID_PARAM;
    
    sqlite3_reset(db->get_stmt);
    sqlite3_bind_blob(db->get_stmt, 1, key, klen, SQLITE_STATIC);
    
    int rc = sqlite3_step(db->get_stmt);
    if (rc == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(db->get_stmt, 0);
        int blob_size = sqlite3_column_bytes(db->get_stmt, 0);
        
        void* data = infra_malloc(blob_size);
        if (!data) return INFRA_ERROR_NO_MEMORY;
        
        memcpy(data, blob, blob_size);
        *val = data;
        *vlen = blob_size;
        return INFRA_OK;
    } else if (rc == SQLITE_DONE) {
        return INFRA_ERROR_NOT_FOUND;
    }
    
    return INFRA_ERROR_IO;
}

infra_error_t poly_sqlite_put(poly_sqlite_db_t* db, const void* key, size_t klen, const void* val, size_t vlen) {
    if (!db || !key || !val) return INFRA_ERROR_INVALID_PARAM;
    
    sqlite3_reset(db->put_stmt);
    sqlite3_bind_blob(db->put_stmt, 1, key, klen, SQLITE_STATIC);
    sqlite3_bind_blob(db->put_stmt, 2, val, vlen, SQLITE_STATIC);
    
    int rc = sqlite3_step(db->put_stmt);
    return (rc == SQLITE_DONE) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t poly_sqlite_del(poly_sqlite_db_t* db, const void* key, size_t klen) {
    if (!db || !key) return INFRA_ERROR_INVALID_PARAM;
    
    sqlite3_reset(db->del_stmt);
    sqlite3_bind_blob(db->del_stmt, 1, key, klen, SQLITE_STATIC);
    
    int rc = sqlite3_step(db->del_stmt);
    return (rc == SQLITE_DONE) ? INFRA_OK : INFRA_ERROR_IO;
}

// 事务操作
infra_error_t poly_sqlite_begin(poly_sqlite_db_t* db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    char* err_msg = NULL;
    int rc = sqlite3_exec(db->db, "BEGIN", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        return INFRA_ERROR_IO;
    }
    return INFRA_OK;
}

infra_error_t poly_sqlite_commit(poly_sqlite_db_t* db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    char* err_msg = NULL;
    int rc = sqlite3_exec(db->db, "COMMIT", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        return INFRA_ERROR_IO;
    }
    return INFRA_OK;
}

infra_error_t poly_sqlite_rollback(poly_sqlite_db_t* db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    char* err_msg = NULL;
    int rc = sqlite3_exec(db->db, "ROLLBACK", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        return INFRA_ERROR_IO;
    }
    return INFRA_OK;
}

// 迭代器
infra_error_t poly_sqlite_iter_create(poly_sqlite_db_t* db, poly_sqlite_iter_t** iter) {
    if (!db || !iter) return INFRA_ERROR_INVALID_PARAM;
    
    poly_sqlite_iter_t* it = infra_malloc(sizeof(poly_sqlite_iter_t));
    if (!it) return INFRA_ERROR_NO_MEMORY;
    
    const char* sql = "SELECT key, value FROM kv";
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &it->stmt, NULL);
    if (rc != SQLITE_OK) {
        infra_free(it);
        return INFRA_ERROR_IO;
    }
    
    it->db = db;
    *iter = it;
    return INFRA_OK;
}

infra_error_t poly_sqlite_iter_next(poly_sqlite_iter_t* iter, void** key, size_t* klen, void** val, size_t* vlen) {
    if (!iter || !key || !klen || !val || !vlen) return INFRA_ERROR_INVALID_PARAM;
    
    int rc = sqlite3_step(iter->stmt);
    if (rc == SQLITE_ROW) {
        // 获取key
        const void* key_blob = sqlite3_column_blob(iter->stmt, 0);
        int key_size = sqlite3_column_bytes(iter->stmt, 0);
        void* key_data = infra_malloc(key_size);
        if (!key_data) return INFRA_ERROR_NO_MEMORY;
        memcpy(key_data, key_blob, key_size);
        *key = key_data;
        *klen = key_size;
        
        // 获取value
        const void* val_blob = sqlite3_column_blob(iter->stmt, 1);
        int val_size = sqlite3_column_bytes(iter->stmt, 1);
        void* val_data = infra_malloc(val_size);
        if (!val_data) {
            infra_free(key_data);
            return INFRA_ERROR_NO_MEMORY;
        }
        memcpy(val_data, val_blob, val_size);
        *val = val_data;
        *vlen = val_size;
        
        return INFRA_OK;
    } else if (rc == SQLITE_DONE) {
        return INFRA_ERROR_NOT_FOUND;
    }
    
    return INFRA_ERROR_IO;
}

infra_error_t poly_sqlite_iter_destroy(poly_sqlite_iter_t* iter) {
    if (!iter) return INFRA_ERROR_INVALID_PARAM;
    
    sqlite3_finalize(iter->stmt);
    infra_free(iter);
    
    return INFRA_OK;
}

// 在文件末尾添加内置插件定义
const poly_builtin_plugin_t g_sqlite_plugin = {
    .name = "sqlite",
    .interface = &g_sqlite_interface,
    .type = POLY_PLUGIN_SQLITE
}; 