#include "internal/infra/infra_core.h"
#include "internal/poly/poly_duckdbkv.h"
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
static infra_error_t poly_duckdb_set_internal(void* handle, const char* key, size_t key_len,
                                   const void* value, size_t value_size);
static infra_error_t poly_duckdb_get_internal(void* handle, const char* key, size_t key_len,
                                   void** value, size_t* value_size);
static infra_error_t poly_duckdb_del_internal(void* handle, const char* key, size_t key_len);

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

// DuckDB数据库句柄
struct poly_duckdb_db {
    duckdb_database db;
    duckdb_connection conn;
    duckdb_prepared_statement stmt;
};

// DuckDB迭代器
struct poly_duckdb_iter {
    duckdb_result result;
    size_t current_row;
    size_t total_rows;
};

// 初始化DuckDB模块
infra_error_t poly_duckdb_init(void** handle) {
    fprintf(stderr, "DuckDB init starting...\n");
    if (!handle) return INFRA_ERROR_INVALID_PARAM;

    // 如果已经初始化，先清理
    if (g_duckdb.handle) {
        cosmo_dlclose(g_duckdb.handle);
        g_duckdb.handle = NULL;
    }

    const char* duckdb_path = "libduckdb.so";//@cosmo_dlopen will auto find .dll/.dylib
    fprintf(stderr, "Attempting to load DuckDB library from: %s\n", duckdb_path);
    g_duckdb.handle = cosmo_dlopen(duckdb_path, RTLD_LAZY);
    if (!g_duckdb.handle) {
        fprintf(stderr, "Failed to load DuckDB library from %s: %s\n", duckdb_path, cosmo_dlerror());
        return INFRA_ERROR_IO;
    }
    fprintf(stderr, "Successfully loaded DuckDB library from %s\n", duckdb_path);

    // 获取函数指针
    #define LOAD_SYMBOL(name) \
        g_duckdb.name = (duckdb_##name##_t)cosmo_dlsym(g_duckdb.handle, "duckdb_" #name); \
        if (!g_duckdb.name) { \
            fprintf(stderr, "Failed to get duckdb_" #name " symbol: %s\n", cosmo_dlerror()); \
            cosmo_dlclose(g_duckdb.handle); \
            g_duckdb.handle = NULL; \
            return INFRA_ERROR_IO; \
        }

    LOAD_SYMBOL(open);
    LOAD_SYMBOL(close);
    LOAD_SYMBOL(connect);
    LOAD_SYMBOL(disconnect);
    LOAD_SYMBOL(query);
    LOAD_SYMBOL(prepare);
    LOAD_SYMBOL(destroy_prepare);
    LOAD_SYMBOL(execute_prepared);
    LOAD_SYMBOL(destroy_result);
    LOAD_SYMBOL(value_is_null);
    LOAD_SYMBOL(value_blob);
    LOAD_SYMBOL(bind_blob);
    LOAD_SYMBOL(bind_varchar);
    LOAD_SYMBOL(row_count);
    LOAD_SYMBOL(value_string);
    LOAD_SYMBOL(free);

    #undef LOAD_SYMBOL

    // 分配新的数据库句柄
    poly_duckdb_db_t* ddb = (poly_duckdb_db_t*)infra_malloc(sizeof(poly_duckdb_db_t));
    if (!ddb) {
        cosmo_dlclose(g_duckdb.handle);
        g_duckdb.handle = NULL;
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(ddb, 0, sizeof(poly_duckdb_db_t));
    *handle = ddb;
    return INFRA_OK;
}

// 清理DuckDB模块
void poly_duckdb_cleanup(void* handle) {
    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
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
    if (db->db) {
        g_duckdb.close(&db->db);
        db->db = NULL;
    }
    infra_free(db);
}

// 打开数据库
infra_error_t poly_duckdb_open(void* handle, const char* path) {
    if (!handle || !path) {
        fprintf(stderr, "Invalid parameters: handle=%p, path=%s\n", 
                handle, path ? path : "NULL");
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* ddb = (poly_duckdb_db_t*)handle;
    fprintf(stderr, "Opening database at path: %s\n", path);

    // 如果路径是 ":memory:" 或 NULL，使用内存数据库
    const char* db_path = (path && strcmp(path, ":memory:") != 0) ? path : NULL;

    // 打开数据库
    duckdb_state state = g_duckdb.open(db_path, &ddb->db);
    if (state != DuckDB_SUCCESS) {
        fprintf(stderr, "Failed to open database: state=%d\n", state);
        return INFRA_ERROR_IO;
    }
    fprintf(stderr, "Database opened successfully\n");

    // 创建连接
    state = g_duckdb.connect(ddb->db, &ddb->conn);
    if (state != DuckDB_SUCCESS) {
        fprintf(stderr, "Failed to create connection: state=%d\n", state);
        g_duckdb.close(&ddb->db);
        return INFRA_ERROR_IO;
    }
    fprintf(stderr, "Connection created successfully\n");

    // 创建表
    duckdb_result result;
    memset(&result, 0, sizeof(result));
    state = g_duckdb.query(ddb->conn,
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "key VARCHAR PRIMARY KEY,"
        "value BLOB"
        ");",
        &result);
    fprintf(stderr, "Table creation query state: %d\n", state);
    g_duckdb.destroy_result(&result);

    if (state != DuckDB_SUCCESS) {
        fprintf(stderr, "Failed to create table: state=%d\n", state);
        g_duckdb.disconnect(&ddb->conn);
        g_duckdb.close(&ddb->db);
        return INFRA_ERROR_IO;
    }
    fprintf(stderr, "Table created successfully\n");

    return INFRA_OK;
}

// 关闭数据库
void poly_duckdb_close(void* handle) {
    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
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
    if (db->db) {
        g_duckdb.close(&db->db);
        db->db = NULL;
    }
    infra_free(db);
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
infra_error_t poly_duckdb_set(void* handle, const char* key, size_t key_len,
                             const void* value, size_t value_len) {
    if (!handle || !key || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    
    // 先删除已存在的键
    duckdb_prepared_statement del_stmt;
    duckdb_state state = g_duckdb.prepare(db->conn, "DELETE FROM kv_store WHERE key = ?", &del_stmt);
    if (state != DuckDB_SUCCESS) {
        return INFRA_ERROR_IO;
    }
    
    // 绑定参数
    char* temp_key = infra_malloc(key_len + 1);
    if (!temp_key) {
        g_duckdb.destroy_prepare(&del_stmt);
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(temp_key, key, key_len);
    temp_key[key_len] = '\0';

    state = g_duckdb.bind_varchar(del_stmt, 1, temp_key);
    if (state != DuckDB_SUCCESS) {
        infra_free(temp_key);
        g_duckdb.destroy_prepare(&del_stmt);
        return INFRA_ERROR_IO;
    }

    // 执行删除
    duckdb_result result;
    state = g_duckdb.execute_prepared(del_stmt, &result);
    g_duckdb.destroy_result(&result);
    g_duckdb.destroy_prepare(&del_stmt);

    if (state != DuckDB_SUCCESS) {
        infra_free(temp_key);
        return INFRA_ERROR_IO;
    }

    // 插入新的键值对
    duckdb_prepared_statement ins_stmt;
    state = g_duckdb.prepare(db->conn, "INSERT INTO kv_store (key, value) VALUES (?, ?)", &ins_stmt);
    if (state != DuckDB_SUCCESS) {
        infra_free(temp_key);
        return INFRA_ERROR_IO;
    }

    // 绑定参数
    state = g_duckdb.bind_varchar(ins_stmt, 1, temp_key);
    infra_free(temp_key);
    if (state != DuckDB_SUCCESS) {
        g_duckdb.destroy_prepare(&ins_stmt);
        return INFRA_ERROR_IO;
    }

    g_duckdb.bind_blob(ins_stmt, 2, value, value_len);
    
    // 执行插入
    state = g_duckdb.execute_prepared(ins_stmt, &result);
    g_duckdb.destroy_result(&result);
    g_duckdb.destroy_prepare(&ins_stmt);

    return (state == DuckDB_SUCCESS) ? INFRA_OK : INFRA_ERROR_IO;
}

// 获取键值对
infra_error_t poly_duckdb_get(void* handle, const char* key, size_t key_len,
                             void** value, size_t* value_len) {
    if (!handle || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;

    // 准备查询语句
    duckdb_prepared_statement stmt;
    duckdb_state state = g_duckdb.prepare(db->conn, "SELECT value FROM kv_store WHERE key = ?", &stmt);
    if (state != DuckDB_SUCCESS) {
        return INFRA_ERROR_IO;
    }

    // 绑定参数
    char* temp_key = infra_malloc(key_len + 1);
    if (!temp_key) {
        g_duckdb.destroy_prepare(&stmt);
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(temp_key, key, key_len);
    temp_key[key_len] = '\0';

    state = g_duckdb.bind_varchar(stmt, 1, temp_key);
    infra_free(temp_key);
    if (state != DuckDB_SUCCESS) {
        g_duckdb.destroy_prepare(&stmt);
        return INFRA_ERROR_IO;
    }

    // 执行查询
    duckdb_result result;
    state = g_duckdb.execute_prepared(stmt, &result);
    g_duckdb.destroy_prepare(&stmt);

    if (state != DuckDB_SUCCESS) {
        return INFRA_ERROR_IO;
    }

    // 检查结果
    idx_t row_count = g_duckdb.row_count(&result);
    if (row_count == 0) {
        g_duckdb.destroy_result(&result);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 获取值
    if (g_duckdb.value_is_null(&result, 0, 0)) {
        g_duckdb.destroy_result(&result);
        return INFRA_ERROR_NOT_FOUND;
    }

    duckdb_blob blob = g_duckdb.value_blob(&result, 0, 0);
    void* data = infra_malloc(blob.size);
    if (!data) {
        g_duckdb.destroy_result(&result);
        return INFRA_ERROR_NO_MEMORY;
    }

    memcpy(data, blob.data, blob.size);
    *value = data;
    *value_len = blob.size;

    g_duckdb.destroy_result(&result);
    return INFRA_OK;
}

// 删除键值对
infra_error_t poly_duckdb_del(void* handle, const char* key, size_t key_len) {
    if (!handle || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;

    // 准备删除语句
    duckdb_prepared_statement stmt;
    duckdb_state state = g_duckdb.prepare(db->conn, "DELETE FROM kv_store WHERE key = ?", &stmt);
    if (state != DuckDB_SUCCESS) {
        return INFRA_ERROR_IO;
    }

    // 绑定参数
    char* temp_key = infra_malloc(key_len + 1);
    if (!temp_key) {
        g_duckdb.destroy_prepare(&stmt);
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(temp_key, key, key_len);
    temp_key[key_len] = '\0';

    state = g_duckdb.bind_varchar(stmt, 1, temp_key);
    infra_free(temp_key);
    if (state != DuckDB_SUCCESS) {
        g_duckdb.destroy_prepare(&stmt);
        return INFRA_ERROR_IO;
    }

    // 执行删除
    duckdb_result result;
    state = g_duckdb.execute_prepared(stmt, &result);
    g_duckdb.destroy_result(&result);
    g_duckdb.destroy_prepare(&stmt);

    return (state == DuckDB_SUCCESS) ? INFRA_OK : INFRA_ERROR_IO;
}

// 创建迭代器
infra_error_t poly_duckdb_iter_create(void* db, void** iter) {
    if (!db || !iter) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* ddb = (poly_duckdb_db_t*)db;
    poly_duckdb_iter_t* iterator = infra_malloc(sizeof(poly_duckdb_iter_t));
    if (!iterator) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(iterator, 0, sizeof(poly_duckdb_iter_t));

    duckdb_state state = g_duckdb.query(ddb->conn, "SELECT key, value FROM kv_store", &iterator->result);
    if (state != DuckDB_SUCCESS) {
        infra_free(iterator);
        return INFRA_ERROR_IO;
    }

    iterator->current_row = 0;
    iterator->total_rows = g_duckdb.row_count(&iterator->result);
    *iter = iterator;
    return INFRA_OK;
}

// 获取下一个键值对
infra_error_t poly_duckdb_iter_next(void* iter, char** key, void** value, size_t* value_len) {
    if (!iter || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_iter_t* iterator = (poly_duckdb_iter_t*)iter;
    if (iterator->current_row >= iterator->total_rows) {
        return INFRA_ERROR_NOT_FOUND;
    }

    // 获取key (VARCHAR类型)
    if (g_duckdb.value_is_null(&iterator->result, 0, iterator->current_row)) {
        return INFRA_ERROR_IO;
    }
    duckdb_string key_str = g_duckdb.value_string(&iterator->result, 0, iterator->current_row);
    char* key_data = infra_malloc(key_str.size + 1);
    if (!key_data) {
        g_duckdb.free((void*)key_str.data);
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(key_data, key_str.data, key_str.size);
    key_data[key_str.size] = '\0';
    g_duckdb.free((void*)key_str.data);

    // 获取value (BLOB类型)
    if (g_duckdb.value_is_null(&iterator->result, 1, iterator->current_row)) {
        infra_free(key_data);
        return INFRA_ERROR_IO;
    }
    duckdb_blob value_blob = g_duckdb.value_blob(&iterator->result, 1, iterator->current_row);
    if (!value_blob.data) {
        infra_free(key_data);
        return INFRA_ERROR_IO;
    }
    void* value_data = infra_malloc(value_blob.size);
    if (!value_data) {
        infra_free(key_data);
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(value_data, value_blob.data, value_blob.size);

    *key = key_data;
    *value = value_data;
    *value_len = value_blob.size;
    
    iterator->current_row++;
    return INFRA_OK;
}

// 销毁迭代器
void poly_duckdb_iter_destroy(void* iter) {
    if (!iter) {
        return;
    }

    poly_duckdb_iter_t* iterator = (poly_duckdb_iter_t*)iter;
    g_duckdb.destroy_result(&iterator->result);
    infra_free(iterator);
}

// 全局 DuckDB 接口实例
const poly_duckdb_interface_t g_duckdb_interface = {
    .init = poly_duckdb_init,
    .cleanup = poly_duckdb_cleanup,
    .open = poly_duckdb_open,
    .close = poly_duckdb_close,
    .get = poly_duckdb_get,
    .set = poly_duckdb_set,
    .del = poly_duckdb_del,
    .exec = poly_duckdb_exec,
    .iter_create = poly_duckdb_iter_create,
    .iter_next = poly_duckdb_iter_next,
    .iter_destroy = poly_duckdb_iter_destroy
};

// DuckDB 插件接口实例
static const poly_plugin_interface_t g_duckdb_plugin_interface = {
    .init = poly_duckdb_init,
    .cleanup = poly_duckdb_cleanup,
    .set = poly_duckdb_set_internal,
    .get = poly_duckdb_get_internal,
    .del = poly_duckdb_del_internal
};

// 获取 DuckDB 插件接口
const poly_plugin_interface_t* poly_duckdb_get_interface(void) {
    return &g_duckdb_plugin_interface;
}

// 内部函数实现
static infra_error_t poly_duckdb_get_internal(void* handle, const char* key, size_t key_len,
                                   void** value, size_t* value_size) {
    if (!handle || !key || !value || !value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    char* temp_key = infra_malloc(key_len + 1);
    if (!temp_key) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(temp_key, key, key_len);
    temp_key[key_len] = '\0';

    infra_error_t err = poly_duckdb_get(db, temp_key, key_len, value, value_size);
    infra_free(temp_key);
    return err;
}

static infra_error_t poly_duckdb_set_internal(void* handle, const char* key, size_t key_len,
                                   const void* value, size_t value_size) {
    if (!handle || !key || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    char* temp_key = infra_malloc(key_len + 1);
    if (!temp_key) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(temp_key, key, key_len);
    temp_key[key_len] = '\0';

    infra_error_t err = poly_duckdb_set(db, temp_key, key_len, value, value_size);
    infra_free(temp_key);
    return err;
}

static infra_error_t poly_duckdb_del_internal(void* handle, const char* key, size_t key_len) {
    if (!handle || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    char* temp_key = infra_malloc(key_len + 1);
    if (!temp_key) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(temp_key, key, key_len);
    temp_key[key_len] = '\0';

    infra_error_t err = poly_duckdb_del(db, temp_key, key_len);
    infra_free(temp_key);
    return err;
}



