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
void poly_duckdb_cleanup(void* handle) {
    if (!handle) return;
    
    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    if (g_duckdb.handle) {
        g_duckdb.destroy_prepare(&db->get_stmt);
        g_duckdb.destroy_prepare(&db->put_stmt);
        g_duckdb.destroy_prepare(&db->del_stmt);
        g_duckdb.disconnect(&db->conn);
        g_duckdb.close(&db->db);
        cosmo_dlclose(g_duckdb.handle);
        g_duckdb.handle = NULL;
    }
    infra_free(db);
}

// 打开数据库
infra_error_t poly_duckdb_open(void* handle, const char* path) {
    if (!handle || !path || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    
    if (g_duckdb.open(path, &db->db) != DuckDBSuccess) {
        return INFRA_ERROR_IO;
    }
    
    if (g_duckdb.connect(db->db, &db->conn) != DuckDBSuccess) {
        g_duckdb.close(&db->db);
        return INFRA_ERROR_IO;
    }
    
    // 创建KV表
    const char* create_sql = 
        "CREATE TABLE IF NOT EXISTS kv ("
        "  key BLOB PRIMARY KEY,"
        "  value BLOB"
        ")";
    duckdb_result result;
    duckdb_state state = g_duckdb.query(db->conn, create_sql, &result);
    g_duckdb.destroy_result(&result);
    if (state != DuckDBSuccess) {
        g_duckdb.disconnect(&db->conn);
        g_duckdb.close(&db->db);
        return INFRA_ERROR_IO;
    }
    
    // 准备语句
    const char* get_sql = "SELECT value FROM kv WHERE key = ?";
    if (g_duckdb.prepare(db->conn, get_sql, &db->get_stmt) != DuckDBSuccess) {
        g_duckdb.disconnect(&db->conn);
        g_duckdb.close(&db->db);
        return INFRA_ERROR_IO;
    }
    
    const char* put_sql = "INSERT OR REPLACE INTO kv (key, value) VALUES (?, ?)";
    if (g_duckdb.prepare(db->conn, put_sql, &db->put_stmt) != DuckDBSuccess) {
        g_duckdb.destroy_prepare(&db->get_stmt);
        g_duckdb.disconnect(&db->conn);
        g_duckdb.close(&db->db);
        return INFRA_ERROR_IO;
    }
    
    const char* del_sql = "DELETE FROM kv WHERE key = ?";
    if (g_duckdb.prepare(db->conn, del_sql, &db->del_stmt) != DuckDBSuccess) {
        g_duckdb.destroy_prepare(&db->get_stmt);
        g_duckdb.destroy_prepare(&db->put_stmt);
        g_duckdb.disconnect(&db->conn);
        g_duckdb.close(&db->db);
        return INFRA_ERROR_IO;
    }
    
    return INFRA_OK;
}

// 关闭数据库
infra_error_t poly_duckdb_close(void* handle) {
    if (!handle || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    g_duckdb.destroy_prepare(&db->get_stmt);
    g_duckdb.destroy_prepare(&db->put_stmt);
    g_duckdb.destroy_prepare(&db->del_stmt);
    g_duckdb.disconnect(&db->conn);
    g_duckdb.close(&db->db);
    
    return INFRA_OK;
}

// 执行 SQL 语句
infra_error_t poly_duckdb_exec(void* handle, const char* sql) {
    if (!handle || !sql || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    duckdb_result result;
    duckdb_state state = g_duckdb.query(db->conn, sql, &result);
    g_duckdb.destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

// KV操作
infra_error_t poly_duckdb_get(void* handle, const char* key, void** value, size_t* value_size) {
    if (!handle || !key || !value || !value_size || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    
    fprintf(stderr, "Getting key: %s (length: %zu)\n", key, strlen(key));
    
    // 绑定参数 - 不包含 null 终止符
    g_duckdb.bind_blob(db->get_stmt, 1, key, strlen(key));
    
    // 执行查询
    duckdb_result result;
    if (g_duckdb.execute_prepared(db->get_stmt, &result) != DuckDBSuccess) {
        fprintf(stderr, "Get operation failed\n");
        return INFRA_ERROR_IO;
    }
    
    // 检查结果
    if (g_duckdb.value_is_null(&result, 0, 0)) {
        fprintf(stderr, "Key not found (null value)\n");
        g_duckdb.destroy_result(&result);
        return INFRA_ERROR_NOT_FOUND;
    }
    
    // 获取BLOB数据
    duckdb_blob blob = g_duckdb.value_blob(&result, 0, 0);
    fprintf(stderr, "Found value with size: %zu\n", blob.size);
    
    // 如果值为空，也视为未找到
    if (blob.size == 0) {
        fprintf(stderr, "Key not found (empty value)\n");
        g_duckdb.destroy_result(&result);
        return INFRA_ERROR_NOT_FOUND;
    }
    
    // 复制数据
    void* data = infra_malloc(blob.size);
    if (!data) {
        g_duckdb.destroy_result(&result);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    memcpy(data, blob.data, blob.size);
    *value = data;
    *value_size = blob.size;
    
    g_duckdb.destroy_result(&result);
    return INFRA_OK;
}

infra_error_t poly_duckdb_set(void* handle, const char* key, const void* value, size_t value_size) {
    if (!handle || !key || !value || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    
    // 绑定参数 - 不包含 null 终止符
    g_duckdb.bind_blob(db->put_stmt, 1, key, strlen(key));
    g_duckdb.bind_blob(db->put_stmt, 2, value, value_size);
    
    // 执行语句
    duckdb_result result;
    duckdb_state state = g_duckdb.execute_prepared(db->put_stmt, &result);
    g_duckdb.destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t poly_duckdb_del(void* handle, const char* key) {
    if (!handle || !key || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    
    fprintf(stderr, "Deleting key: %s (length: %zu)\n", key, strlen(key));
    
    // 绑定参数 - 不包含 null 终止符
    g_duckdb.bind_blob(db->del_stmt, 1, key, strlen(key));
    
    // 执行语句
    duckdb_result result;
    duckdb_state state = g_duckdb.execute_prepared(db->del_stmt, &result);
    fprintf(stderr, "Delete operation state: %d\n", state);
    g_duckdb.destroy_result(&result);
    
    return (state == DuckDBSuccess) ? INFRA_OK : INFRA_ERROR_IO;
}

// 创建迭代器
infra_error_t poly_duckdb_iter_create(void* handle, void** iter) {
    if (!handle || !iter || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_db_t* db = (poly_duckdb_db_t*)handle;
    poly_duckdb_iter_t* iterator = infra_malloc(sizeof(poly_duckdb_iter_t));
    if (!iterator) return INFRA_ERROR_NO_MEMORY;
    
    iterator->db = db;
    iterator->current_row = 0;
    iterator->total_rows = 0;
    
    // 准备查询语句
    const char* sql = "SELECT key, value FROM kv ORDER BY key";
    if (g_duckdb.prepare(db->conn, sql, &iterator->stmt) != DuckDBSuccess) {
        infra_free(iterator);
        return INFRA_ERROR_IO;
    }
    
    // 执行查询
    if (g_duckdb.execute_prepared(iterator->stmt, &iterator->result) != DuckDBSuccess) {
        g_duckdb.destroy_prepare(&iterator->stmt);
        infra_free(iterator);
        return INFRA_ERROR_IO;
    }
    
    iterator->total_rows = g_duckdb.row_count(&iterator->result);
    *iter = iterator;
    return INFRA_OK;
}

// 获取下一个键值对
infra_error_t poly_duckdb_iter_next(void* iter, char** key, void** value, size_t* value_size) {
    if (!iter || !key || !value || !value_size || !g_duckdb.handle) return INFRA_ERROR_INVALID_PARAM;
    
    poly_duckdb_iter_t* iterator = (poly_duckdb_iter_t*)iter;
    
    // 检查是否已经遍历完所有行
    if (iterator->current_row >= iterator->total_rows) {
        return INFRA_ERROR_NOT_FOUND;
    }
    
    // 获取当前行的键和值
    duckdb_blob key_blob = g_duckdb.value_blob(&iterator->result, 0, iterator->current_row);
    duckdb_blob value_blob = g_duckdb.value_blob(&iterator->result, 1, iterator->current_row);
    
    // 分配内存并复制键 - 添加 null 终止符
    char* key_data = infra_malloc(key_blob.size + 1);
    if (!key_data) return INFRA_ERROR_NO_MEMORY;
    memcpy(key_data, key_blob.data, key_blob.size);
    key_data[key_blob.size] = '\0';  // 确保字符串正确终止
    
    // 分配内存并复制值
    void* value_data = infra_malloc(value_blob.size);
    if (!value_data) {
        infra_free(key_data);
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(value_data, value_blob.data, value_blob.size);
    
    *key = key_data;
    *value = value_data;
    *value_size = value_blob.size;
    
    iterator->current_row++;
    return INFRA_OK;
}

// 销毁迭代器
void poly_duckdb_iter_destroy(void* iter) {
    if (!iter || !g_duckdb.handle) return;
    
    poly_duckdb_iter_t* iterator = (poly_duckdb_iter_t*)iter;
    g_duckdb.destroy_result(&iterator->result);
    g_duckdb.destroy_prepare(&iterator->stmt);
    infra_free(iterator);
}

// 全局 DuckDB 接口实例
const poly_duckdb_interface_t g_duckdb_interface = {
    .init = poly_duckdb_init,
    .cleanup = poly_duckdb_cleanup,
    .open = poly_duckdb_open,
    .close = poly_duckdb_close,
    .exec = poly_duckdb_exec,
    .get = poly_duckdb_get,
    .set = poly_duckdb_set,
    .del = poly_duckdb_del,
    .iter_create = poly_duckdb_iter_create,
    .iter_next = poly_duckdb_iter_next,
    .iter_destroy = poly_duckdb_iter_destroy
}; 
