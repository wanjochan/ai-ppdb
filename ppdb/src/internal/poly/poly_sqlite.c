#include "internal/poly/poly_sqlite.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "sqlite3.h"

// 内部函数声明
static infra_error_t poly_sqlite_init(void** handle);
static void poly_sqlite_cleanup(void* handle);
static infra_error_t poly_sqlite_open_internal(void* handle, const char* path);
static infra_error_t poly_sqlite_close_internal(void* handle);
static infra_error_t poly_sqlite_exec_internal(void* handle, const char* sql);
static infra_error_t poly_sqlite_get_internal(void* handle, const char* key, void** value, size_t* value_size);
static infra_error_t poly_sqlite_set_internal(void* handle, const char* key, const void* value, size_t value_size);
static infra_error_t poly_sqlite_del_internal(void* handle, const char* key);
static infra_error_t poly_sqlite_iter_create_internal(void* handle, void** iter);
static infra_error_t poly_sqlite_iter_next_internal(void* iter, char** key, void** value, size_t* value_size);
static void poly_sqlite_iter_destroy_internal(void* iter);

// SQLite 接口实例
const poly_sqlite_interface_t g_sqlite_interface = {
    .init = poly_sqlite_init,
    .cleanup = poly_sqlite_cleanup,
    .open = poly_sqlite_open_internal,
    .close = poly_sqlite_close_internal,
    .exec = poly_sqlite_exec_internal,
    .get = poly_sqlite_get_internal,
    .set = poly_sqlite_set_internal,
    .del = poly_sqlite_del_internal,
    .iter_create = poly_sqlite_iter_create_internal,
    .iter_next = poly_sqlite_iter_next_internal,
    .iter_destroy = poly_sqlite_iter_destroy_internal
};

// SQLite 插件接口实例
static const poly_plugin_interface_t g_sqlite_plugin_interface = {
    .init = poly_sqlite_init,
    .cleanup = poly_sqlite_cleanup,
    .set = poly_sqlite_set_internal,
    .get = poly_sqlite_get_internal,
    .del = poly_sqlite_del_internal
};

// 获取SQLite插件接口
const poly_plugin_interface_t* poly_sqlite_get_interface(void) {
    return &g_sqlite_plugin_interface;
}

// SQLite 数据库句柄
struct poly_sqlite_db {
    sqlite3* db;
};

// SQLite 迭代器
struct poly_sqlite_iter {
    sqlite3_stmt* stmt;
    poly_sqlite_db_t* db;
};

// 打开数据库
infra_error_t poly_sqlite_open(poly_sqlite_db_t** db, const char* path) {
    if (!db || !path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 分配内存
    poly_sqlite_db_t* new_db = infra_malloc(sizeof(poly_sqlite_db_t));
    if (!new_db) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 打开数据库
    int rc = sqlite3_open(path, &new_db->db);
    if (rc != SQLITE_OK) {
        infra_free(new_db);
        return INFRA_ERROR_IO;
    }

    // 创建表
    const char* sql = "CREATE TABLE IF NOT EXISTS kv_store (key BLOB PRIMARY KEY, value BLOB);";
    char* err_msg = NULL;
    rc = sqlite3_exec(new_db->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        sqlite3_close(new_db->db);
        infra_free(new_db);
        return INFRA_ERROR_IO;
    }

    *db = new_db;
    return INFRA_OK;
}

// 关闭数据库
void poly_sqlite_close(poly_sqlite_db_t* db) {
    if (!db) {
        return;
    }

    if (db->db) {
        sqlite3_close(db->db);
    }
    infra_free(db);
}

// 设置键值对
infra_error_t poly_sqlite_set(poly_sqlite_db_t* db, const char* key, size_t key_len, const void* value, size_t value_len) {
    if (!db || !key || !value || key_len == 0 || value_len == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 准备 SQL 语句
    const char* sql = "INSERT OR REPLACE INTO kv_store (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_IO;
    }

    // 绑定参数
    rc = sqlite3_bind_blob(stmt, 1, key, key_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    rc = sqlite3_bind_blob(stmt, 2, value, value_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    // 执行语句
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? INFRA_OK : INFRA_ERROR_IO;
}

// 获取键值对
infra_error_t poly_sqlite_get(poly_sqlite_db_t* db, const char* key, size_t key_len, void** value, size_t* value_len) {
    if (!db || !key || !value || !value_len || key_len == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 准备 SQL 语句
    const char* sql = "SELECT value FROM kv_store WHERE key = ?;";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_IO;
    }

    // 绑定参数
    rc = sqlite3_bind_blob(stmt, 1, key, key_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    // 执行查询
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        // 获取值
        const void* blob = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);

        // 分配内存并复制数据
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
infra_error_t poly_sqlite_del(poly_sqlite_db_t* db, const char* key, size_t key_len) {
    if (!db || !key || key_len == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 准备 SQL 语句
    const char* sql = "DELETE FROM kv_store WHERE key = ?;";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_IO;
    }

    // 绑定参数
    rc = sqlite3_bind_blob(stmt, 1, key, key_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return INFRA_ERROR_IO;
    }

    // 执行语句
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? INFRA_OK : INFRA_ERROR_IO;
}

// 创建迭代器
infra_error_t poly_sqlite_iter_create(poly_sqlite_db_t* db, poly_sqlite_iter_t** iter) {
    if (!db || !iter) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 分配内存
    poly_sqlite_iter_t* new_iter = infra_malloc(sizeof(poly_sqlite_iter_t));
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
infra_error_t poly_sqlite_iter_next(poly_sqlite_iter_t* iter, char** key, size_t* key_len, void** value, size_t* value_len) {
    if (!iter || !key || !key_len || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 执行查询
    int rc = sqlite3_step(iter->stmt);
    if (rc == SQLITE_ROW) {
        // 获取键
        const void* key_blob = sqlite3_column_blob(iter->stmt, 0);
        int key_size = sqlite3_column_bytes(iter->stmt, 0);

        // 分配内存并复制键
        char* key_data = infra_malloc(key_size + 1);  // +1 for null terminator
        if (!key_data) {
            return INFRA_ERROR_NO_MEMORY;
        }
        memcpy(key_data, key_blob, key_size);
        key_data[key_size] = '\0';
        *key = key_data;
        *key_len = key_size;

        // 获取值
        const void* value_blob = sqlite3_column_blob(iter->stmt, 1);
        int value_size = sqlite3_column_bytes(iter->stmt, 1);

        // 分配内存并复制值
        void* value_data = infra_malloc(value_size);
        if (!value_data) {
            infra_free(key_data);
            return INFRA_ERROR_NO_MEMORY;
        }
        memcpy(value_data, value_blob, value_size);
        *value = value_data;
        *value_len = value_size;

        return INFRA_OK;
    }

    return INFRA_ERROR_NOT_FOUND;
}

// 销毁迭代器
void poly_sqlite_iter_destroy(poly_sqlite_iter_t* iter) {
    if (!iter) {
        return;
    }

    if (iter->stmt) {
        sqlite3_finalize(iter->stmt);
    }
    infra_free(iter);
}

// 初始化
static infra_error_t poly_sqlite_init(void** handle) {
    infra_error_t err = INFRA_OK;
    poly_sqlite_ctx_t* ctx = NULL;

    // 初始化 SQLite
    if (sqlite3_initialize() != SQLITE_OK) {
        return INFRA_ERROR_SYSTEM;
    }

    // 分配上下文
    ctx = (poly_sqlite_ctx_t*)malloc(sizeof(poly_sqlite_ctx_t));
    if (ctx == NULL) {
        sqlite3_shutdown();
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(ctx, 0, sizeof(poly_sqlite_ctx_t));

    // 打开内存数据库
    int rc = sqlite3_open_v2(":memory:", &ctx->db, 
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        NULL);
    if (rc != SQLITE_OK) {
        free(ctx);
        sqlite3_shutdown();
        return INFRA_ERROR_SYSTEM;
    }

    // 创建表
    rc = sqlite3_exec(ctx->db,
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "key BLOB PRIMARY KEY,"
        "value BLOB"
        ");", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close_v2(ctx->db);
        free(ctx);
        sqlite3_shutdown();
        return INFRA_ERROR_SYSTEM;
    }

    // 准备语句
    const char* sql;
    
    // GET
    sql = "SELECT value FROM kv_store WHERE key = ?;";
    rc = sqlite3_prepare_v2(ctx->db, sql, -1, &ctx->get_stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close_v2(ctx->db);
        free(ctx);
        sqlite3_shutdown();
        return INFRA_ERROR_SYSTEM;
    }

    // SET
    sql = "INSERT OR REPLACE INTO kv_store (key, value) VALUES (?, ?);";
    rc = sqlite3_prepare_v2(ctx->db, sql, -1, &ctx->set_stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(ctx->get_stmt);
        sqlite3_close_v2(ctx->db);
        free(ctx);
        sqlite3_shutdown();
        return INFRA_ERROR_SYSTEM;
    }

    // DEL
    sql = "DELETE FROM kv_store WHERE key = ?;";
    rc = sqlite3_prepare_v2(ctx->db, sql, -1, &ctx->del_stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(ctx->get_stmt);
        sqlite3_finalize(ctx->set_stmt);
        sqlite3_close_v2(ctx->db);
        free(ctx);
        sqlite3_shutdown();
        return INFRA_ERROR_SYSTEM;
    }

    // ITER
    sql = "SELECT key, value FROM kv_store;";
    rc = sqlite3_prepare_v2(ctx->db, sql, -1, &ctx->iter_stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(ctx->get_stmt);
        sqlite3_finalize(ctx->set_stmt);
        sqlite3_finalize(ctx->del_stmt);
        sqlite3_close_v2(ctx->db);
        free(ctx);
        sqlite3_shutdown();
        return INFRA_ERROR_SYSTEM;
    }

    *handle = ctx;
    return err;
}

// 清理
static void poly_sqlite_cleanup(void* handle) {
    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)handle;
    if (ctx) {
        if (ctx->get_stmt) sqlite3_finalize(ctx->get_stmt);
        if (ctx->set_stmt) sqlite3_finalize(ctx->set_stmt);
        if (ctx->del_stmt) sqlite3_finalize(ctx->del_stmt);
        if (ctx->iter_stmt) sqlite3_finalize(ctx->iter_stmt);
        if (ctx->db) sqlite3_close_v2(ctx->db);
        free(ctx);
    }
    sqlite3_shutdown();
}

// 执行SQL
infra_error_t poly_sqlite_exec(poly_sqlite_ctx_t* ctx, const char* sql) {
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
static infra_error_t poly_sqlite_open_internal(void* handle, const char* path) {
    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)handle;
    int rc;

    // 打开数据库
    rc = sqlite3_open_v2(path, &ctx->db, 
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_SYSTEM;
    }

    // 创建表
    rc = sqlite3_exec(ctx->db,
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "key BLOB PRIMARY KEY,"
        "value BLOB"
        ");", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return INFRA_ERROR_SYSTEM;
    }

    // 准备语句
    const char* sql;
    
    // GET
    sql = "SELECT value FROM kv_store WHERE key = ?;";
    rc = sqlite3_prepare_v2(ctx->db, sql, -1, &ctx->get_stmt, NULL);
    if (rc != SQLITE_OK) return INFRA_ERROR_SYSTEM;

    // SET
    sql = "INSERT OR REPLACE INTO kv_store (key, value) VALUES (?, ?);";
    rc = sqlite3_prepare_v2(ctx->db, sql, -1, &ctx->set_stmt, NULL);
    if (rc != SQLITE_OK) return INFRA_ERROR_SYSTEM;

    // DEL
    sql = "DELETE FROM kv_store WHERE key = ?;";
    rc = sqlite3_prepare_v2(ctx->db, sql, -1, &ctx->del_stmt, NULL);
    if (rc != SQLITE_OK) return INFRA_ERROR_SYSTEM;

    // ITER
    sql = "SELECT key, value FROM kv_store;";
    rc = sqlite3_prepare_v2(ctx->db, sql, -1, &ctx->iter_stmt, NULL);
    if (rc != SQLITE_OK) return INFRA_ERROR_SYSTEM;

    return INFRA_OK;
}

static infra_error_t poly_sqlite_close_internal(void* handle) {
    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)handle;
    
    if (ctx->get_stmt) sqlite3_finalize(ctx->get_stmt);
    if (ctx->set_stmt) sqlite3_finalize(ctx->set_stmt);
    if (ctx->del_stmt) sqlite3_finalize(ctx->del_stmt);
    if (ctx->iter_stmt) sqlite3_finalize(ctx->iter_stmt);
    
    if (ctx->db) sqlite3_close_v2(ctx->db);
    ctx->db = NULL;
    
    return INFRA_OK;
}

static infra_error_t poly_sqlite_exec_internal(void* handle, const char* sql) {
    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)handle;
    int rc = sqlite3_exec(ctx->db, sql, NULL, NULL, NULL);
    return (rc == SQLITE_OK) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

static infra_error_t poly_sqlite_get_internal(void* handle, const char* key,
    void** value, size_t* value_size) {
    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)handle;
    int rc;
    
    // 重置语句
    sqlite3_reset(ctx->get_stmt);
    sqlite3_clear_bindings(ctx->get_stmt);
    
    // 绑定参数
    rc = sqlite3_bind_text(ctx->get_stmt, 1, key, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) return INFRA_ERROR_SYSTEM;
    
    // 执行查询
    rc = sqlite3_step(ctx->get_stmt);
    
    if (rc == SQLITE_ROW) {
        // 获取值
        const void* blob = sqlite3_column_blob(ctx->get_stmt, 0);
        int size = sqlite3_column_bytes(ctx->get_stmt, 0);
        
        // 分配内存
        void* buf = malloc(size);
        if (!buf) {
            sqlite3_reset(ctx->get_stmt);
            sqlite3_clear_bindings(ctx->get_stmt);
            return INFRA_ERROR_NO_MEMORY;
        }
        
        // 复制数据
        memcpy(buf, blob, size);
        *value = buf;
        *value_size = size;
        
        sqlite3_reset(ctx->get_stmt);
        sqlite3_clear_bindings(ctx->get_stmt);
        return INFRA_OK;
    }
    
    sqlite3_reset(ctx->get_stmt);
    sqlite3_clear_bindings(ctx->get_stmt);
    return INFRA_ERROR_NOT_FOUND;
}

static infra_error_t poly_sqlite_set_internal(void* handle, const char* key,
    const void* value, size_t value_size) {
    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)handle;
    int rc;
    
    // 重置语句
    sqlite3_reset(ctx->set_stmt);
    sqlite3_clear_bindings(ctx->set_stmt);
    
    // 绑定参数
    rc = sqlite3_bind_text(ctx->set_stmt, 1, key, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) return INFRA_ERROR_SYSTEM;
    
    rc = sqlite3_bind_blob(ctx->set_stmt, 2, value, value_size, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) return INFRA_ERROR_SYSTEM;
    
    // 执行语句
    rc = sqlite3_step(ctx->set_stmt);
    
    sqlite3_reset(ctx->set_stmt);
    sqlite3_clear_bindings(ctx->set_stmt);
    
    return (rc == SQLITE_DONE) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

static infra_error_t poly_sqlite_del_internal(void* handle, const char* key) {
    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)handle;
    int rc;
    
    // 重置语句
    sqlite3_reset(ctx->del_stmt);
    sqlite3_clear_bindings(ctx->del_stmt);
    
    // 绑定参数
    rc = sqlite3_bind_text(ctx->del_stmt, 1, key, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) return INFRA_ERROR_SYSTEM;
    
    // 执行语句
    rc = sqlite3_step(ctx->del_stmt);
    
    sqlite3_reset(ctx->del_stmt);
    sqlite3_clear_bindings(ctx->del_stmt);
    
    return (rc == SQLITE_DONE) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

static infra_error_t poly_sqlite_iter_create_internal(void* handle, void** iter) {
    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)handle;
    
    // 重置语句
    sqlite3_reset(ctx->iter_stmt);
    sqlite3_clear_bindings(ctx->iter_stmt);
    
    *iter = ctx->iter_stmt;
    return INFRA_OK;
}

static infra_error_t poly_sqlite_iter_next_internal(void* iter, char** key,
    void** value, size_t* value_size) {
    sqlite3_stmt* stmt = (sqlite3_stmt*)iter;
    int rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        // 获取键
        const char* key_text = (const char*)sqlite3_column_text(stmt, 0);
        *key = strdup(key_text);
        if (!*key) return INFRA_ERROR_NO_MEMORY;
        
        // 获取值
        const void* blob = sqlite3_column_blob(stmt, 1);
        int size = sqlite3_column_bytes(stmt, 1);
        
        void* buf = malloc(size);
        if (!buf) {
            free(*key);
            return INFRA_ERROR_NO_MEMORY;
        }
        
        memcpy(buf, blob, size);
        *value = buf;
        *value_size = size;
        
        return INFRA_OK;
    }
    
    return INFRA_ERROR_NOT_FOUND;
}

static void poly_sqlite_iter_destroy_internal(void* iter) {
    sqlite3_stmt* stmt = (sqlite3_stmt*)iter;
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
} 