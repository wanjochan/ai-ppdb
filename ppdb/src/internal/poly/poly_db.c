#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "internal/poly/poly_db.h"
#include "sqlite3.h"  // SQLite 头文件
#include "duckdb.h"   // DuckDB 头文件，仅用于类型定义，实现通过动态加载

// DuckDB 函数指针类型定义
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

// DuckDB 实现结构体
typedef struct duckdb_impl {
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
} duckdb_impl_t;

// SQLite 实现结构体
typedef struct sqlite_impl {
    sqlite3* db;
} sqlite_impl_t;

// 数据库结果集结构体
typedef struct poly_db_result {
    poly_db_t* db;
    void* internal_result;
} poly_db_result_t;

// 多态数据库结构体
typedef struct poly_db {
    poly_db_type_t type;
    void* impl;
    infra_error_t (*exec)(poly_db_t* db, const char* sql);
    infra_error_t (*query)(poly_db_t* db, const char* sql, poly_db_result_t** result);
    void (*close)(poly_db_t* db);
    infra_error_t (*result_row_count)(poly_db_result_t* result, size_t* count);
    infra_error_t (*result_get_blob)(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size);
    infra_error_t (*result_get_string)(poly_db_result_t* result, size_t row, size_t col, char** str);
} poly_db_t;

// 修改错误码（稍后再清理）
#define INFRA_ERROR_LOAD_LIBRARY INFRA_ERROR_NOT_READY
#define INFRA_ERROR_EXEC_FAILED INFRA_ERROR_QUERY_FAILED

// 创建 DuckDB 实例
static infra_error_t create_duckdb(duckdb_impl_t** impl) {
    if (!impl) return INFRA_ERROR_INVALID_PARAM;

    duckdb_impl_t* duckdb = infra_malloc(sizeof(duckdb_impl_t));
    if (!duckdb) return INFRA_ERROR_NO_MEMORY;
    memset(duckdb, 0, sizeof(duckdb_impl_t));

    // 加载 DuckDB 动态库 (NOTES: cosmo will load dll/dylib automatically)
    duckdb->handle = cosmo_dlopen("libduckdb.so", RTLD_LAZY);
    if (!duckdb->handle) {
        infra_free(duckdb);
        return INFRA_ERROR_LOAD_LIBRARY;
    }

    // 加载函数指针
    duckdb->open = (duckdb_open_t)dlsym(duckdb->handle, "duckdb_open");
    duckdb->close = (duckdb_close_t)dlsym(duckdb->handle, "duckdb_close");
    duckdb->connect = (duckdb_connect_t)dlsym(duckdb->handle, "duckdb_connect");
    duckdb->disconnect = (duckdb_disconnect_t)dlsym(duckdb->handle, "duckdb_disconnect");
    duckdb->query = (duckdb_query_t)dlsym(duckdb->handle, "duckdb_query");
    duckdb->prepare = (duckdb_prepare_t)dlsym(duckdb->handle, "duckdb_prepare");
    duckdb->destroy_prepare = (duckdb_destroy_prepare_t)dlsym(duckdb->handle, "duckdb_destroy_prepare");
    duckdb->execute_prepared = (duckdb_execute_prepared_t)dlsym(duckdb->handle, "duckdb_execute_prepared");
    duckdb->destroy_result = (duckdb_destroy_result_t)dlsym(duckdb->handle, "duckdb_destroy_result");
    duckdb->value_is_null = (duckdb_value_is_null_t)dlsym(duckdb->handle, "duckdb_value_is_null");
    duckdb->value_blob = (duckdb_value_blob_t)dlsym(duckdb->handle, "duckdb_value_blob");
    duckdb->bind_blob = (duckdb_bind_blob_t)dlsym(duckdb->handle, "duckdb_bind_blob");
    duckdb->bind_varchar = (duckdb_bind_varchar_t)dlsym(duckdb->handle, "duckdb_bind_varchar");
    duckdb->row_count = (duckdb_row_count_t)dlsym(duckdb->handle, "duckdb_row_count");
    duckdb->value_string = (duckdb_value_string_t)dlsym(duckdb->handle, "duckdb_value_string");
    duckdb->free = (duckdb_free_t)dlsym(duckdb->handle, "duckdb_free");

    // 验证所有函数指针都已加载
    if (!duckdb->open || !duckdb->close || !duckdb->connect || !duckdb->disconnect ||
        !duckdb->query || !duckdb->prepare || !duckdb->destroy_prepare || !duckdb->execute_prepared ||
        !duckdb->destroy_result || !duckdb->value_is_null || !duckdb->value_blob || !duckdb->bind_blob ||
        !duckdb->bind_varchar || !duckdb->row_count || !duckdb->value_string || !duckdb->free) {
        dlclose(duckdb->handle);
        infra_free(duckdb);
        return INFRA_ERROR_LOAD_LIBRARY;
    }

    *impl = duckdb;
    return INFRA_OK;
}

// 销毁 DuckDB 实例
static void destroy_duckdb(duckdb_impl_t* impl) {
    if (!impl) return;
    if (impl->handle) dlclose(impl->handle);
    infra_free(impl);
}

// DuckDB 实现的接口函数
static infra_error_t poly_duckdb_exec(poly_db_t* db, const char* sql) {
    if (!db || !sql) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)db->impl;

    duckdb_connection conn;
    duckdb_state state = impl->connect(impl->handle, &conn);
    if (state != DuckDBSuccess) {
        printf("DuckDB connect failed\n");
        return INFRA_ERROR_EXEC_FAILED;
    }

    duckdb_result result;
    state = impl->query(conn, sql, &result);
    if (state != DuckDBSuccess) {
        impl->disconnect(&conn);
        printf("DuckDB query failed\n");
        return INFRA_ERROR_EXEC_FAILED;
    }

    impl->destroy_result(&result);
    impl->disconnect(&conn);
    return INFRA_OK;
}

static infra_error_t poly_duckdb_query(poly_db_t* db, const char* sql, poly_db_result_t** result) {
    if (!db || !sql || !result) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)db->impl;
    
    duckdb_connection conn;
    duckdb_state state = impl->connect(impl->handle, &conn);
    if (state != DuckDBSuccess) {
        printf("DuckDB connect failed\n");
        return INFRA_ERROR_EXEC_FAILED;
    }
    
    poly_db_result_t* res = infra_malloc(sizeof(poly_db_result_t));
    if (!res) {
        impl->disconnect(&conn);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    duckdb_result* duck_result = infra_malloc(sizeof(duckdb_result));
    if (!duck_result) {
        impl->disconnect(&conn);
        infra_free(res);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    state = impl->query(conn, sql, duck_result);
    if (state != DuckDBSuccess) {
        impl->disconnect(&conn);
        infra_free(duck_result);
        infra_free(res);
        printf("DuckDB query failed\n");
        return INFRA_ERROR_EXEC_FAILED;
    }
    
    res->db = db;
    res->internal_result = duck_result;
    *result = res;

    impl->disconnect(&conn);
    return INFRA_OK;
}

static infra_error_t poly_duckdb_result_row_count(poly_db_result_t* result, size_t* count) {
    if (!result || !count) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
    duckdb_result* duck_result = (duckdb_result*)result->internal_result;
    *count = impl->row_count(duck_result);
    return INFRA_OK;
}

static infra_error_t poly_duckdb_result_get_blob(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size) {
    if (!result || !data || !size) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
    duckdb_result* duck_result = (duckdb_result*)result->internal_result;
    
    if (impl->value_is_null(duck_result, col, row)) {
        *data = NULL;
        *size = 0;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    duckdb_blob blob = impl->value_blob(duck_result, col, row);
    if (!blob.data) {
        *data = NULL;
        *size = 0;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    void* blob_copy = infra_malloc(blob.size);
    if (!blob_copy) return INFRA_ERROR_NO_MEMORY;
    
    memcpy(blob_copy, blob.data, blob.size);
    *data = blob_copy;
    *size = blob.size;
    
    return INFRA_OK;
}

static infra_error_t poly_duckdb_result_get_string(poly_db_result_t* result, size_t row, size_t col, char** str) {
    if (!result || !str) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
    duckdb_result* duck_result = (duckdb_result*)result->internal_result;
    
    if (impl->value_is_null(duck_result, col, row)) {
        *str = NULL;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    duckdb_string value = impl->value_string(duck_result, col, row);
    if (!value.data) {
        *str = NULL;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    *str = infra_strdup(value.data);
    if (!*str) return INFRA_ERROR_NO_MEMORY;
    
    return INFRA_OK;
}

static void poly_duckdb_close(poly_db_t* db) {
    if (!db) return;
    duckdb_impl_t* impl = (duckdb_impl_t*)db->impl;
    if (impl) {
        if (impl->handle) {
            impl->close(impl->handle);
        }
        destroy_duckdb(impl);
    }
    infra_free(db);
}

// SQLite 实现的接口函数
static infra_error_t sqlite_exec(poly_db_t* db, const char* sql) {
    if (!db || !sql) return INFRA_ERROR_INVALID_PARAM;
    sqlite_impl_t* impl = (sqlite_impl_t*)db->impl;
    char* err_msg = NULL;
    int rc = sqlite3_exec(impl->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        printf("SQLite error: %s\n", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
        return INFRA_ERROR_EXEC_FAILED;
    }
    return INFRA_OK;
}

static infra_error_t sqlite_query(poly_db_t* db, const char* sql, poly_db_result_t** result) {
    if (!db || !sql || !result) return INFRA_ERROR_INVALID_PARAM;
    sqlite_impl_t* impl = (sqlite_impl_t*)db->impl;
    
    poly_db_result_t* res = infra_malloc(sizeof(poly_db_result_t));
    if (!res) return INFRA_ERROR_NO_MEMORY;
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(impl->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        infra_free(res);
        return INFRA_ERROR_EXEC_FAILED;
    }
    
    res->db = db;
    res->internal_result = stmt;
    *result = res;
    return INFRA_OK;
}

static infra_error_t sqlite_result_row_count(poly_db_result_t* result, size_t* count) {
    if (!result || !count) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* stmt = (sqlite3_stmt*)result->internal_result;
    
    // 重置语句以便重新执行
    sqlite3_reset(stmt);
    
    // 计算行数
    size_t rows = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rows++;
    }
    
    // 重置语句以便后续使用
    sqlite3_reset(stmt);
    
    *count = rows;
    return INFRA_OK;
}

static infra_error_t sqlite_result_get_blob(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size) {
    if (!result || !data || !size) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* stmt = (sqlite3_stmt*)result->internal_result;
    
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        *data = NULL;
        *size = 0;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    const void* blob = sqlite3_column_blob(stmt, col);
    int blob_size = sqlite3_column_bytes(stmt, col);
    
    if (!blob) {
        *data = NULL;
        *size = 0;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    void* blob_copy = infra_malloc(blob_size);
    if (!blob_copy) return INFRA_ERROR_NO_MEMORY;
    
    memcpy(blob_copy, blob, blob_size);
    *data = blob_copy;
    *size = blob_size;
    
    return INFRA_OK;
}

static infra_error_t sqlite_result_get_string(poly_db_result_t* result, size_t row, size_t col, char** str) {
    if (!result || !str) return INFRA_ERROR_INVALID_PARAM;
    sqlite3_stmt* stmt = (sqlite3_stmt*)result->internal_result;
    
    // 重置语句
    sqlite3_reset(stmt);
    
    // 定位到指定行
    for (size_t i = 0; i <= row; i++) {
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            *str = NULL;
            return INFRA_ERROR_NOT_FOUND;
        }
    }
    
    const unsigned char* text = sqlite3_column_text(stmt, col);
    if (!text) {
        *str = NULL;
        return INFRA_ERROR_NOT_FOUND;
    }
    
    *str = infra_strdup((const char*)text);
    if (!*str) return INFRA_ERROR_NO_MEMORY;
    
    return INFRA_OK;
}

static void sqlite_close(poly_db_t* db) {
    if (!db || !db->impl) return;
    sqlite_impl_t* impl = (sqlite_impl_t*)db->impl;
    if (impl->db) {
        sqlite3_close(impl->db);
    }
    infra_free(impl);
    infra_free(db);
}

// 修改 poly_db_open 函数
infra_error_t poly_db_open(const poly_db_config_t* config, poly_db_t** db) {
    if (!config || !db) return INFRA_ERROR_INVALID_PARAM;

    poly_db_t* new_db = infra_malloc(sizeof(poly_db_t));
    if (!new_db) return INFRA_ERROR_NO_MEMORY;
    memset(new_db, 0, sizeof(poly_db_t));

    new_db->type = config->type;

    switch (config->type) {
        case POLY_DB_TYPE_SQLITE: {
            sqlite_impl_t* impl = infra_malloc(sizeof(sqlite_impl_t));
            if (!impl) {
                infra_free(new_db);
                return INFRA_ERROR_NO_MEMORY;
            }

            int rc = sqlite3_open(config->url, &impl->db);
            if (rc != SQLITE_OK) {
                infra_free(impl);
                infra_free(new_db);
                return INFRA_ERROR_EXEC_FAILED;
            }

            new_db->impl = impl;
            new_db->exec = sqlite_exec;
            new_db->query = sqlite_query;
            new_db->close = sqlite_close;
            new_db->result_row_count = sqlite_result_row_count;
            new_db->result_get_blob = sqlite_result_get_blob;
            new_db->result_get_string = sqlite_result_get_string;
            break;
        }
        case POLY_DB_TYPE_DUCKDB: {
            duckdb_impl_t* impl;
            infra_error_t err = create_duckdb(&impl);
            if (err != INFRA_OK) {
                infra_free(new_db);
                return err;
            }

            duckdb_database db;
            duckdb_state state = impl->open(config->url, &db);
            if (state != DuckDBSuccess) {
                destroy_duckdb(impl);
                infra_free(new_db);
                return INFRA_ERROR_EXEC_FAILED;
            }

            impl->handle = db;
            new_db->impl = impl;
            new_db->exec = poly_duckdb_exec;
            new_db->query = poly_duckdb_query;
            new_db->close = poly_duckdb_close;
            new_db->result_row_count = poly_duckdb_result_row_count;
            new_db->result_get_blob = poly_duckdb_result_get_blob;
            new_db->result_get_string = poly_duckdb_result_get_string;
            break;
        }
        default:
            infra_free(new_db);
            return INFRA_ERROR_INVALID_PARAM;
    }

    *db = new_db;
    return INFRA_OK;
}

infra_error_t poly_db_close(poly_db_t* db) {
    if (db == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (db->close) {
        db->close(db);
    }
    return INFRA_OK;
}

infra_error_t poly_db_result_free(poly_db_result_t* result) {
    if (result == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    if (result->internal_result) {
        if (result->db->type == POLY_DB_TYPE_SQLITE) {
            sqlite3_finalize(result->internal_result);
        } else if (result->db->type == POLY_DB_TYPE_DUCKDB) {
            duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
            impl->destroy_result(result->internal_result);
        }
    }
    
    infra_free(result);
    return INFRA_OK;
}

infra_error_t poly_db_result_row_count(poly_db_result_t* result, size_t* count) {
    if (!result || !count) return INFRA_ERROR_INVALID_PARAM;
    
    if (result->db->result_row_count) {
        return result->db->result_row_count(result, count);
    }
    
    return INFRA_ERROR_NOT_SUPPORTED;
}

infra_error_t poly_db_result_get_blob(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size) {
    if (!result || !data || !size) return INFRA_ERROR_INVALID_PARAM;
    
    if (result->db->result_get_blob) {
        return result->db->result_get_blob(result, row, col, data, size);
    }
    
    return INFRA_ERROR_NOT_SUPPORTED;
}

infra_error_t poly_db_result_get_string(poly_db_result_t* result, size_t row, size_t col, char** str) {
    if (!result || !str) return INFRA_ERROR_INVALID_PARAM;
    
    if (result->db->result_get_string) {
        return result->db->result_get_string(result, row, col, str);
    }
    
    return INFRA_ERROR_NOT_SUPPORTED;
}

poly_db_type_t poly_db_get_type(const poly_db_t* db) {
    if (!db) return POLY_DB_TYPE_UNKNOWN;
    return db->type;
}

infra_error_t poly_db_exec(poly_db_t* db, const char* sql) {
    if (!db || !sql) return INFRA_ERROR_INVALID_PARAM;
    if (!db->exec) return INFRA_ERROR_NOT_SUPPORTED;
    return db->exec(db, sql);
}

infra_error_t poly_db_query(poly_db_t* db, const char* sql, poly_db_result_t** result) {
    if (!db || !sql || !result) return INFRA_ERROR_INVALID_PARAM;
    if (!db->query) return INFRA_ERROR_NOT_SUPPORTED;
    return db->query(db, sql, result);
}
