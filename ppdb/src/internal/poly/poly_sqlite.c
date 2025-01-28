#include "internal/poly/poly_sqlite.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "sqlite3.h"

// SQLite 上下文
typedef struct poly_sqlite_ctx {
    sqlite3* db;
    sqlite3_stmt* get_stmt;
    sqlite3_stmt* set_stmt;
    sqlite3_stmt* del_stmt;
    sqlite3_stmt* iter_stmt;
} poly_sqlite_ctx_t;

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

// 打开数据库
static infra_error_t poly_sqlite_open(void* handle, const char* path) {
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

// 关闭数据库
static infra_error_t poly_sqlite_close(void* handle) {
    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)handle;
    
    if (ctx->get_stmt) sqlite3_finalize(ctx->get_stmt);
    if (ctx->set_stmt) sqlite3_finalize(ctx->set_stmt);
    if (ctx->del_stmt) sqlite3_finalize(ctx->del_stmt);
    if (ctx->iter_stmt) sqlite3_finalize(ctx->iter_stmt);
    
    if (ctx->db) sqlite3_close_v2(ctx->db);
    ctx->db = NULL;
    
    return INFRA_OK;
}

// 执行SQL
static infra_error_t poly_sqlite_exec(void* handle, const char* sql) {
    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)handle;
    int rc = sqlite3_exec(ctx->db, sql, NULL, NULL, NULL);
    return (rc == SQLITE_OK) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

// 获取键值对
static infra_error_t poly_sqlite_get(void* handle, const char* key,
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

// 设置键值对
static infra_error_t poly_sqlite_set(void* handle, const char* key,
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
    if (rc != SQLITE_OK) {
        sqlite3_reset(ctx->set_stmt);
        sqlite3_clear_bindings(ctx->set_stmt);
        return INFRA_ERROR_SYSTEM;
    }
    
    // 执行语句
    rc = sqlite3_step(ctx->set_stmt);
    sqlite3_reset(ctx->set_stmt);
    sqlite3_clear_bindings(ctx->set_stmt);
    
    return (rc == SQLITE_DONE) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

// 删除键值对
static infra_error_t poly_sqlite_del(void* handle, const char* key) {
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

// 创建迭代器
static infra_error_t poly_sqlite_iter_create(void* handle, void** iter) {
    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)handle;
    
    // 重置迭代器语句
    sqlite3_reset(ctx->iter_stmt);
    
    *iter = ctx->iter_stmt;
    return INFRA_OK;
}

// 迭代下一个
static infra_error_t poly_sqlite_iter_next(void* iter, char** key, void** value, size_t* value_size) {
    sqlite3_stmt* stmt = (sqlite3_stmt*)iter;
    int rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        // 获取 key
        const char* key_text = (const char*)sqlite3_column_text(stmt, 0);
        *key = strdup(key_text);
        if (!*key) return INFRA_ERROR_NO_MEMORY;
        
        // 获取 value
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

// 销毁迭代器
static void poly_sqlite_iter_destroy(void* iter) {
    // 迭代器是 stmt，在 cleanup 时统一释放
}

// SQLite 插件接口实例
static const poly_plugin_interface_t g_sqlite_plugin_interface = {
    .init = poly_sqlite_init,
    .cleanup = poly_sqlite_cleanup,
    .set = poly_sqlite_set,
    .get = poly_sqlite_get,
    .del = poly_sqlite_del
};

// 获取SQLite插件接口
const poly_plugin_interface_t* poly_sqlite_get_interface(void) {
    return &g_sqlite_plugin_interface;
} 