#include "internal/poly/poly_sqlitekv.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "sqlite3.h"

// 内部函数声明
static infra_error_t poly_sqlitekv_init(void** handle);
static void poly_sqlitekv_cleanup(void* handle);
static infra_error_t poly_sqlitekv_open_internal(void* handle, const char* path);
static infra_error_t poly_sqlitekv_close_internal(void* handle);
static infra_error_t poly_sqlitekv_exec_internal(void* handle, const char* sql);
static infra_error_t poly_sqlitekv_get_internal(void* handle, const char* key, size_t key_len, void** value, size_t* value_size);
static infra_error_t poly_sqlitekv_set_internal(void* handle, const char* key, size_t key_len, const void* value, size_t value_size);
static infra_error_t poly_sqlitekv_del_internal(void* handle, const char* key, size_t key_len);
static infra_error_t poly_sqlitekv_iter_create_internal(void* handle, void** iter);
static infra_error_t poly_sqlitekv_iter_next_internal(void* iter, char** key, void** value, size_t* value_size);
static void poly_sqlitekv_iter_destroy_internal(void* iter);

// SQLite KV 接口实例
const poly_sqlitekv_interface_t g_sqlitekv_interface = {
    .init = poly_sqlitekv_init,
    .cleanup = poly_sqlitekv_cleanup,
    .open = poly_sqlitekv_open_internal,
    .close = poly_sqlitekv_close_internal,
    .exec = poly_sqlitekv_exec_internal,
    .get = poly_sqlitekv_get_internal,
    .set = poly_sqlitekv_set_internal,
    .del = poly_sqlitekv_del_internal,
    .iter_create = poly_sqlitekv_iter_create_internal,
    .iter_next = poly_sqlitekv_iter_next_internal,
    .iter_destroy = poly_sqlitekv_iter_destroy_internal
};

// SQLite KV 插件接口实例
static const poly_plugin_interface_t g_sqlitekv_plugin_interface = {
    .init = poly_sqlitekv_init,
    .cleanup = poly_sqlitekv_cleanup,
    .set = poly_sqlitekv_set_internal,
    .get = poly_sqlitekv_get_internal,
    .del = poly_sqlitekv_del_internal
};

// 获取SQLite KV插件接口
const poly_plugin_interface_t* poly_sqlitekv_get_interface(void) {
    return &g_sqlitekv_plugin_interface;
}

// SQLite KV 数据库句柄
struct poly_sqlitekv_db {
    sqlite3* db;
};

// SQLite KV 迭代器
struct poly_sqlitekv_iter {
    sqlite3_stmt* stmt;
    poly_sqlitekv_db_t* db;
};

// 打开数据库
infra_error_t poly_sqlitekv_open(poly_sqlitekv_db_t** db, const char* path) {
    if (!db || !path) {
        printf("Invalid parameters: db=%p, path=%s\n", (void*)db, path ? path : "NULL");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 分配内存
    poly_sqlitekv_db_t* new_db = infra_malloc(sizeof(poly_sqlitekv_db_t));
    if (!new_db) {
        printf("Failed to allocate memory for database handle\n");
        return INFRA_ERROR_NO_MEMORY;
    }

    // 打开数据库
    int rc;
    if (strcmp(path, ":memory:") == 0) {
        rc = sqlite3_open(":memory:", &new_db->db);
    } else {
        printf("sqlite3_open_v2 %s\n", path);
        rc = sqlite3_open_v2(path, &new_db->db, 
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
            NULL);
    }

    if (rc != SQLITE_OK) {
        printf("Failed to open database: %s\n", sqlite3_errmsg(new_db->db));
        infra_free(new_db);
        return INFRA_ERROR_IO;
    }

    // Enable foreign keys and WAL mode for better transaction support
    char* err_msg = NULL;
    rc = sqlite3_exec(new_db->db, "PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        printf("Failed to set pragmas: %s\n", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
        sqlite3_close(new_db->db);
        infra_free(new_db);
        return INFRA_ERROR_IO;
    }

    // Create table with key as TEXT to make it easier to work with SQL statements
    const char* sql = "CREATE TABLE IF NOT EXISTS kv_store (key TEXT PRIMARY KEY, value BLOB);";
    rc = sqlite3_exec(new_db->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        printf("Failed to create table: %s\n", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
        sqlite3_close(new_db->db);
        infra_free(new_db);
        return INFRA_ERROR_IO;
    }

    *db = new_db;
    return INFRA_OK;
}

// 关闭数据库
void poly_sqlitekv_close(poly_sqlitekv_db_t* db) {
    if (!db) {
        return;
    }

    if (db->db) {
        sqlite3_close(db->db);
    }
    infra_free(db);
}

// 设置键值对
infra_error_t poly_sqlitekv_set(void* handle, const char* key, size_t key_len,
                             const void* value, size_t value_len) {
    if (!handle || !key || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_sqlitekv_db_t* db = (poly_sqlitekv_db_t*)handle;
    sqlite3_stmt* stmt = NULL;
    int rc;

    // Replace existing key-value pair
    rc = sqlite3_prepare_v2(db->db, "REPLACE INTO kv_store (key, value) VALUES (?, ?)", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_IO;
    }

    // Store key as text
    rc = sqlite3_bind_text(stmt, 1, key, key_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    rc = sqlite3_bind_blob(stmt, 2, value, value_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? INFRA_OK : INFRA_ERROR_IO;
}

// 获取键值对
infra_error_t poly_sqlitekv_get(void* handle, const char* key, size_t key_len,
                             void** value, size_t* value_len) {
    if (!handle || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_sqlitekv_db_t* db = (poly_sqlitekv_db_t*)handle;
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(db->db, "SELECT value FROM kv_store WHERE key = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_IO;
    }

    // Store key as text
    rc = sqlite3_bind_text(stmt, 1, key, key_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);

        void* data = infra_malloc(blob_size);
        if (!data) {
            sqlite3_finalize(stmt);
            return INFRA_ERROR_NO_MEMORY;
        }

        memcpy(data, blob, blob_size);
        *value = data;
        *value_len = blob_size;
        sqlite3_finalize(stmt);
        return INFRA_OK;
    }

    sqlite3_finalize(stmt);
    return INFRA_ERROR_NOT_FOUND;
}

// 删除键值对
infra_error_t poly_sqlitekv_del(void* handle, const char* key, size_t key_len) {
    if (!handle || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_sqlitekv_db_t* db = (poly_sqlitekv_db_t*)handle;
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(db->db, "DELETE FROM kv_store WHERE key = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_IO;
    }

    // Store key as text
    rc = sqlite3_bind_text(stmt, 1, key, key_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? INFRA_OK : INFRA_ERROR_IO;
}

// 创建迭代器
infra_error_t poly_sqlitekv_iter_create(poly_sqlitekv_db_t* db, poly_sqlitekv_iter_t** iter) {
    if (!db || !iter) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 分配内存
    poly_sqlitekv_iter_t* new_iter = infra_malloc(sizeof(poly_sqlitekv_iter_t));
    if (!new_iter) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 准备 SQL 语句
    const char* sql = "SELECT key, value FROM kv_store;";
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &new_iter->stmt, NULL);
    if (rc != SQLITE_OK) {
        infra_free(new_iter);
        return INFRA_ERROR_IO;
    }

    new_iter->db = db;
    *iter = new_iter;
    return INFRA_OK;
}

// 迭代下一个键值对
infra_error_t poly_sqlitekv_iter_next(poly_sqlitekv_iter_t* iter, char** key, size_t* key_len, void** value, size_t* value_len) {
    if (!iter || !key || !key_len || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 执行查询
    int rc = sqlite3_step(iter->stmt);
    if (rc == SQLITE_ROW) {
        // 获取键
        const char* key_text = (const char*)sqlite3_column_text(iter->stmt, 0);
        const void* value_blob = sqlite3_column_blob(iter->stmt, 1);
        int value_blob_size = sqlite3_column_bytes(iter->stmt, 1);

        char* key_data = infra_malloc(strlen(key_text) + 1);
        void* value_data = infra_malloc(value_blob_size);
        if (!key_data || !value_data) {
            if (key_data) infra_free(key_data);
            if (value_data) infra_free(value_data);
            return INFRA_ERROR_NO_MEMORY;
        }

        strcpy(key_data, key_text);
        memcpy(value_data, value_blob, value_blob_size);

        *key = key_data;
        *key_len = strlen(key_text);
        *value = value_data;
        *value_len = value_blob_size;
        return INFRA_OK;
    }

    return INFRA_ERROR_NOT_FOUND;
}

// 销毁迭代器
void poly_sqlitekv_iter_destroy(poly_sqlitekv_iter_t* iter) {
    if (!iter) {
        return;
    }

    if (iter->stmt) {
        sqlite3_finalize(iter->stmt);
    }
    infra_free(iter);
}

// 初始化
static infra_error_t poly_sqlitekv_init(void** handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_sqlitekv_db_t* db = infra_malloc(sizeof(poly_sqlitekv_db_t));
    if (!db) {
        return INFRA_ERROR_NO_MEMORY;
    }

    memset(db, 0, sizeof(poly_sqlitekv_db_t));
    *handle = db;
    return INFRA_OK;
}

// 清理
static void poly_sqlitekv_cleanup(void* handle) {
    if (!handle) {
        return;
    }

    poly_sqlitekv_db_t* db = (poly_sqlitekv_db_t*)handle;
    if (db->db) {
        sqlite3_close(db->db);
    }
    infra_free(db);
}

// 执行SQL
infra_error_t poly_sqlitekv_exec(poly_sqlitekv_ctx_t* ctx, const char* sql) {
    if (!ctx || !sql) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    char* err_msg = NULL;
    int rc = sqlite3_exec(ctx->db, sql, NULL, NULL, &err_msg);
    
    if (rc != SQLITE_OK) {
        if (err_msg) {
            sqlite3_free(err_msg);
        }
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

// 内部函数实现
static infra_error_t poly_sqlitekv_open_internal(void* handle, const char* path) {
    if (!handle || !path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_sqlitekv_db_t* db = (poly_sqlitekv_db_t*)handle;
    int rc;
    
    if (strcmp(path, ":memory:") == 0) {
        rc = sqlite3_open(":memory:", &db->db);
    } else {
        rc = sqlite3_open_v2(path, &db->db, 
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
            NULL);
    }

    if (rc != SQLITE_OK) {
        printf("Failed to open database: %s\n", sqlite3_errmsg(db->db));
        return INFRA_ERROR_IO;
    }

    // Enable foreign keys and WAL mode for better transaction support
    char* err_msg = NULL;
    rc = sqlite3_exec(db->db, "PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        printf("Failed to set pragmas: %s\n", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
        sqlite3_close(db->db);
        return INFRA_ERROR_IO;
    }

    // Create table with key as TEXT to make it easier to work with SQL statements
    const char* sql = "CREATE TABLE IF NOT EXISTS kv_store (key TEXT PRIMARY KEY, value BLOB);";
    rc = sqlite3_exec(db->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        printf("Failed to create table: %s\n", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
        sqlite3_close(db->db);
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

static infra_error_t poly_sqlitekv_close_internal(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_sqlitekv_db_t* db = (poly_sqlitekv_db_t*)handle;
    if (db->db) {
        sqlite3_close(db->db);
        db->db = NULL;
    }
    infra_free(db);
    return INFRA_OK;
}

static infra_error_t poly_sqlitekv_exec_internal(void* handle, const char* sql) {
    if (!handle || !sql) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_sqlitekv_db_t* db = (poly_sqlitekv_db_t*)handle;
    char* err_msg = NULL;
    int rc = sqlite3_exec(db->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        printf("Failed to execute SQL: %s\n", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
        
        // Check if this is a transaction-related error
        if (strstr(sql, "BEGIN") != NULL || strstr(sql, "COMMIT") != NULL || strstr(sql, "ROLLBACK") != NULL) {
            return INFRA_ERROR_IO;
        }
        
        // For other errors, try to rollback if we're in a transaction
        sqlite3_exec(db->db, "ROLLBACK", NULL, NULL, NULL);
        return INFRA_ERROR_IO;
    }
    return INFRA_OK;
}

static infra_error_t poly_sqlitekv_get_internal(void* handle, const char* key, size_t key_len,
                                  void** value, size_t* value_size) {
    if (!handle || !key || !value || !value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_sqlitekv_db_t* db = (poly_sqlitekv_db_t*)handle;
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(db->db, "SELECT value FROM kv_store WHERE key = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_IO;
    }

    // Store key as text
    rc = sqlite3_bind_text(stmt, 1, key, key_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);

        void* data = infra_malloc(blob_size);
        if (!data) {
            sqlite3_finalize(stmt);
            return INFRA_ERROR_NO_MEMORY;
        }

        memcpy(data, blob, blob_size);
        *value = data;
        *value_size = blob_size;
        sqlite3_finalize(stmt);
        return INFRA_OK;
    }

    sqlite3_finalize(stmt);
    return INFRA_ERROR_NOT_FOUND;
}

static infra_error_t poly_sqlitekv_set_internal(void* handle, const char* key, size_t key_len,
                                  const void* value, size_t value_size) {
    if (!handle || !key || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_sqlitekv_db_t* db = (poly_sqlitekv_db_t*)handle;
    sqlite3_stmt* stmt = NULL;
    int rc;

    // Check if we're in a transaction
    sqlite3_stmt* check_stmt = NULL;
    rc = sqlite3_prepare_v2(db->db, "SELECT COUNT(*) FROM sqlite_master", -1, &check_stmt, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_IO;
    }
    rc = sqlite3_step(check_stmt);
    sqlite3_finalize(check_stmt);

    // If we can't read from the database, it means we're in a transaction that has failed
    if (rc != SQLITE_ROW) {
        return INFRA_ERROR_IO;
    }

    // Replace existing key-value pair
    rc = sqlite3_prepare_v2(db->db, "REPLACE INTO kv_store (key, value) VALUES (?, ?)", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_IO;
    }

    // Store key as text
    rc = sqlite3_bind_text(stmt, 1, key, key_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    rc = sqlite3_bind_blob(stmt, 2, value, value_size, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

static infra_error_t poly_sqlitekv_del_internal(void* handle, const char* key, size_t key_len) {
    if (!handle || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_sqlitekv_db_t* db = (poly_sqlitekv_db_t*)handle;
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(db->db, "DELETE FROM kv_store WHERE key = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_IO;
    }

    // Store key as text
    rc = sqlite3_bind_text(stmt, 1, key, key_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? INFRA_OK : INFRA_ERROR_IO;
}

static infra_error_t poly_sqlitekv_iter_create_internal(void* handle, void** iter) {
    if (!handle || !iter) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_sqlitekv_db_t* db = (poly_sqlitekv_db_t*)handle;
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(db->db, "SELECT key, value FROM kv_store", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_IO;
    }

    *iter = stmt;
    return INFRA_OK;
}

static infra_error_t poly_sqlitekv_iter_next_internal(void* iter, char** key, void** value, size_t* value_size) {
    if (!iter || !key || !value || !value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    sqlite3_stmt* stmt = (sqlite3_stmt*)iter;
    int rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        const char* key_text = (const char*)sqlite3_column_text(stmt, 0);
        const void* value_blob = sqlite3_column_blob(stmt, 1);
        int value_blob_size = sqlite3_column_bytes(stmt, 1);

        char* key_data = infra_malloc(strlen(key_text) + 1);
        void* value_data = infra_malloc(value_blob_size);
        if (!key_data || !value_data) {
            if (key_data) infra_free(key_data);
            if (value_data) infra_free(value_data);
            return INFRA_ERROR_NO_MEMORY;
        }

        strcpy(key_data, key_text);
        memcpy(value_data, value_blob, value_blob_size);

        *key = key_data;
        *value = value_data;
        *value_size = value_blob_size;
        return INFRA_OK;
    }

    return INFRA_ERROR_NOT_FOUND;
}

static void poly_sqlitekv_iter_destroy_internal(void* iter) {
    if (!iter) {
        return;
    }

    sqlite3_stmt* stmt = (sqlite3_stmt*)iter;
    sqlite3_finalize(stmt);
}
