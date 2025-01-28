#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "internal/poly/poly_db.h"
#include "sqlite3.h"
// #include "duckdb.h" //we use cosmo_dlxxxx load libduckdb

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

// Database interface structure
struct poly_db {
    poly_db_type_t type;
    void* impl;
    infra_error_t (*exec)(struct poly_db* db, const char* sql);
    infra_error_t (*query)(struct poly_db* db, const char* sql, poly_db_result_t** result);
    void (*free_result)(struct poly_db* db, void* result);
    void (*close)(struct poly_db* db);
    infra_error_t (*result_row_count)(poly_db_result_t* result, size_t* count);
    infra_error_t (*result_get_blob)(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size);
    infra_error_t (*result_get_string)(poly_db_result_t* result, size_t row, size_t col, char** str);
};

// Result structure
struct poly_db_result {
    poly_db_t* db;
    void* internal_result;
};

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
static infra_error_t duckdb_exec(poly_db_t* db, const char* sql) {
    if (!db || !sql) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)db->impl;
    duckdb_result result;
    duckdb_state state = impl->query(impl->handle, sql, &result);
    if (state != DuckDBSuccess) return INFRA_ERROR_EXEC_FAILED;
    impl->destroy_result(&result);
    return INFRA_OK;
}

static infra_error_t duckdb_query(poly_db_t* db, const char* sql, poly_db_result_t** result) {
    if (!db || !sql || !result) return INFRA_ERROR_INVALID_PARAM;
    duckdb_impl_t* impl = (duckdb_impl_t*)db->impl;
    
    poly_db_result_t* res = infra_malloc(sizeof(poly_db_result_t));
    if (!res) return INFRA_ERROR_NO_MEMORY;
    
    duckdb_result* duck_result = infra_malloc(sizeof(duckdb_result));
    if (!duck_result) {
        infra_free(res);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    duckdb_state state = impl->query(impl->handle, sql, duck_result);
    if (state != DuckDBSuccess) {
        infra_free(duck_result);
        infra_free(res);
        return INFRA_ERROR_EXEC_FAILED;
    }
    
    res->db = db;
    res->internal_result = duck_result;
    *result = res;
    return INFRA_OK;
}

// 公共接口函数实现
infra_error_t poly_db_open(const char* url, poly_db_t** db) {
    if (!url || !db) return INFRA_ERROR_INVALID_PARAM;

    // 解析 URL 获取数据库类型和路径
    poly_db_type_t type = POLY_DB_TYPE_UNKNOWN;
    const char* path = NULL;

    if (strncmp(url, "sqlite://", 9) == 0) {
        type = POLY_DB_TYPE_SQLITE;
        path = url + 9;
    } else if (strncmp(url, "duckdb://", 9) == 0) {
        type = POLY_DB_TYPE_DUCKDB;
        path = url + 9;
    } else {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建数据库实例
    poly_db_t* new_db = infra_malloc(sizeof(poly_db_t));
    if (!new_db) return INFRA_ERROR_NO_MEMORY;
    memset(new_db, 0, sizeof(poly_db_t));
    new_db->type = type;

    // 根据类型初始化实现
    infra_error_t err = INFRA_OK;
    switch (type) {
        case POLY_DB_TYPE_DUCKDB: {
            duckdb_impl_t* impl;
            err = create_duckdb(&impl);
            if (err != INFRA_OK) {
                infra_free(new_db);
                return err;
            }
            new_db->impl = impl;
            new_db->exec = duckdb_exec;
            new_db->query = duckdb_query;
            break;
        }
        case POLY_DB_TYPE_SQLITE:
            // TODO: 实现 SQLite 支持
            err = INFRA_ERROR_NOT_IMPLEMENTED;
            break;
        default:
            err = INFRA_ERROR_INVALID_PARAM;
    }

    if (err != INFRA_OK) {
        infra_free(new_db);
        return err;
    }

    *db = new_db;
    return INFRA_OK;
}

void poly_db_close(poly_db_t* db) {
    if (!db) return;
    
    switch (db->type) {
        case POLY_DB_TYPE_DUCKDB:
            destroy_duckdb((duckdb_impl_t*)db->impl);
            break;
        case POLY_DB_TYPE_SQLITE:
            // TODO: 实现 SQLite 清理
            break;
        default:
            break;
    }
    
    infra_free(db);
}

void poly_db_result_free(poly_db_result_t* result) {
    if (!result) return;
    if (result->db && result->internal_result) {
        switch (result->db->type) {
            case POLY_DB_TYPE_DUCKDB: {
                duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
                impl->destroy_result((duckdb_result*)result->internal_result);
                break;
            }
            case POLY_DB_TYPE_SQLITE:
                // TODO: 实现 SQLite 结果清理
                break;
            default:
                break;
        }
        infra_free(result->internal_result);
    }
    infra_free(result);
}

infra_error_t poly_db_result_row_count(poly_db_result_t* result, size_t* count) {
    if (!result || !count) return INFRA_ERROR_INVALID_PARAM;
    
    switch (result->db->type) {
        case POLY_DB_TYPE_DUCKDB: {
            duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
            *count = impl->row_count((duckdb_result*)result->internal_result);
            return INFRA_OK;
        }
        case POLY_DB_TYPE_SQLITE:
            // TODO: 实现 SQLite 行数获取
            return INFRA_ERROR_NOT_IMPLEMENTED;
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }
}

infra_error_t poly_db_result_get_blob(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size) {
    if (!result || !data || !size) return INFRA_ERROR_INVALID_PARAM;
    
    switch (result->db->type) {
        case POLY_DB_TYPE_DUCKDB: {
            duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
            duckdb_blob blob = impl->value_blob((duckdb_result*)result->internal_result, col, row);
            *data = infra_malloc(blob.size);
            if (!*data) return INFRA_ERROR_NO_MEMORY;
            memcpy(*data, blob.data, blob.size);
            *size = blob.size;
            return INFRA_OK;
        }
        case POLY_DB_TYPE_SQLITE:
            // TODO: 实现 SQLite BLOB 获取
            return INFRA_ERROR_NOT_IMPLEMENTED;
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }
}

infra_error_t poly_db_result_get_string(poly_db_result_t* result, size_t row, size_t col, char** str) {
    if (!result || !str) return INFRA_ERROR_INVALID_PARAM;
    
    switch (result->db->type) {
        case POLY_DB_TYPE_DUCKDB: {
            duckdb_impl_t* impl = (duckdb_impl_t*)result->db->impl;
            duckdb_string value = impl->value_string((duckdb_result*)result->internal_result, col, row);
            *str = infra_strdup(value);
            if (!*str) return INFRA_ERROR_NO_MEMORY;
            return INFRA_OK;
        }
        case POLY_DB_TYPE_SQLITE:
            // TODO: 实现 SQLite 字符串获取
            return INFRA_ERROR_NOT_IMPLEMENTED;
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }
}
