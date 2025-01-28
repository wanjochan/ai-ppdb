#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "internal/poly/poly_db.h"
#include "cosmo.h"

// DuckDB 类型定义
typedef void* duckdb_database;
typedef void* duckdb_connection;
typedef void* duckdb_result;

// DuckDB 状态
typedef int duckdb_state;
#define DuckDBSuccess 0
#define DuckDBError 1

// DuckDB 函数指针类型定义
typedef duckdb_state (*duckdb_open_t)(const char*, duckdb_database*);
typedef void (*duckdb_close_t)(duckdb_database*);
typedef duckdb_state (*duckdb_connect_t)(duckdb_database, duckdb_connection*);
typedef void (*duckdb_disconnect_t)(duckdb_connection*);
typedef duckdb_state (*duckdb_query_t)(duckdb_connection, const char*, duckdb_result*);
typedef void (*duckdb_destroy_result_t)(duckdb_result*);

// DuckDB 函数指针结构
typedef struct {
    void* handle;
    duckdb_open_t open;
    duckdb_close_t close;
    duckdb_connect_t connect;
    duckdb_disconnect_t disconnect;
    duckdb_query_t query;
    duckdb_destroy_result_t destroy_result;
} duckdb_functions_t;

// DuckDB 数据库句柄
typedef struct {
    duckdb_database db;
    duckdb_connection conn;
    duckdb_functions_t* funcs;
} poly_duckdb_t;

// SQLite 数据库句柄
typedef struct {
    void* db;
} poly_sqlite_t;

// 统一的数据库句柄
typedef struct poly_db {
    poly_db_type_t type;
    union {
        poly_sqlite_t sqlite;
        poly_duckdb_t duckdb;
    };
} poly_db_t;

// URL 解析函数 （成熟后放 poly_db.h 中可以更多地方用到）
static infra_error_t parse_db_url(const char* url, char** scheme, char** path, char** params) {
    if (!url || !scheme || !path || !params) {
        fprintf(stderr, "parse_db_url: Invalid parameters\n");
        return INFRA_ERROR_INVALID_PARAM;
    }

    fprintf(stderr, "parse_db_url: Parsing URL: %s\n", url);

    const char* scheme_end = strstr(url, "://");
    if (!scheme_end) {
        fprintf(stderr, "parse_db_url: Invalid URL format, missing ://\n");
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t scheme_len = scheme_end - url;
    *scheme = (char*)infra_malloc(scheme_len + 1);
    if (!*scheme) {
        fprintf(stderr, "parse_db_url: Failed to allocate memory for scheme\n");
        return INFRA_ERROR_NO_MEMORY;
    }
    strncpy(*scheme, url, scheme_len);
    (*scheme)[scheme_len] = '\0';
    fprintf(stderr, "parse_db_url: Found scheme: %s\n", *scheme);

    const char* path_start = scheme_end + 3;
    const char* params_start = strchr(path_start, '?');
    
    if (params_start) {
        size_t path_len = params_start - path_start;
        *path = (char*)infra_malloc(path_len + 1);
        if (!*path) {
            fprintf(stderr, "parse_db_url: Failed to allocate memory for path\n");
            infra_free(*scheme);
            return INFRA_ERROR_NO_MEMORY;
        }
        strncpy(*path, path_start, path_len);
        (*path)[path_len] = '\0';
        fprintf(stderr, "parse_db_url: Found path: %s\n", *path);

        size_t params_len = strlen(params_start + 1);
        *params = (char*)infra_malloc(params_len + 1);
        if (!*params) {
            fprintf(stderr, "parse_db_url: Failed to allocate memory for params\n");
            infra_free(*scheme);
            infra_free(*path);
            return INFRA_ERROR_NO_MEMORY;
        }
        strcpy(*params, params_start + 1);
        fprintf(stderr, "parse_db_url: Found params: %s\n", *params);
    } else {
        size_t path_len = strlen(path_start);
        *path = (char*)infra_malloc(path_len + 1);
        if (!*path) {
            fprintf(stderr, "parse_db_url: Failed to allocate memory for path\n");
            infra_free(*scheme);
            return INFRA_ERROR_NO_MEMORY;
        }
        strcpy(*path, path_start);
        fprintf(stderr, "parse_db_url: Found path: %s\n", *path);

        *params = (char*)infra_malloc(1);
        if (!*params) {
            fprintf(stderr, "parse_db_url: Failed to allocate memory for params\n");
            infra_free(*scheme);
            infra_free(*path);
            return INFRA_ERROR_NO_MEMORY;
        }
        (*params)[0] = '\0';
        fprintf(stderr, "parse_db_url: No params found\n");
    }

    return INFRA_OK;
}

// DuckDB 动态加载
static infra_error_t load_duckdb(const char* lib_path, duckdb_functions_t** funcs) {
    if (!lib_path || !funcs) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    duckdb_functions_t* f = (duckdb_functions_t*)infra_malloc(sizeof(duckdb_functions_t));
    if (!f) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(f, 0, sizeof(duckdb_functions_t));

    fprintf(stderr, "Loading DuckDB from: %s\n", lib_path);
    f->handle = cosmo_dlopen(lib_path, RTLD_LAZY);
    if (!f->handle) {
        fprintf(stderr, "Failed to load DuckDB: %s\n", cosmo_dlerror());
        infra_free(f);
        return INFRA_ERROR_NOT_FOUND;
    }
    fprintf(stderr, "Successfully loaded DuckDB library\n");

    // 加载所需的函数
    #define LOAD_SYMBOL(name) \
        f->name = (duckdb_##name##_t)cosmo_dlsym(f->handle, "duckdb_" #name); \
        fprintf(stderr, "Loading symbol duckdb_%s: %p\n", #name, (void*)f->name); \
        if (!f->name) { \
            fprintf(stderr, "Failed to get duckdb_" #name " symbol: %s\n", dlerror()); \
            void* handle = f->handle; \
            cosmo_dlclose(handle); \
            infra_free(f); \
            return INFRA_ERROR_IO; \
        }

    LOAD_SYMBOL(open);
    LOAD_SYMBOL(close);
    LOAD_SYMBOL(connect);
    LOAD_SYMBOL(disconnect);
    LOAD_SYMBOL(query);
    LOAD_SYMBOL(destroy_result);

    #undef LOAD_SYMBOL

    fprintf(stderr, "Successfully loaded all DuckDB symbols\n");

    *funcs = f;
    return INFRA_OK;
}

// 获取参数值
static char* get_param_value(const char* params, const char* key) {
    size_t params_len = strlen(params);
    char* params_copy = (char*)infra_malloc(params_len + 1);
    if (!params_copy) return NULL;
    strcpy(params_copy, params);

    char* param = strtok(params_copy, "&");
    while (param) {
        char* equals = strchr(param, '=');
        if (equals) {
            *equals = '\0';
            if (strcmp(param, key) == 0) {
                size_t value_len = strlen(equals + 1);
                char* value = (char*)infra_malloc(value_len + 1);
                if (!value) {
                    infra_free(params_copy);
                    return NULL;
                }
                strcpy(value, equals + 1);
                infra_free(params_copy);
                return value;
            }
        }
        param = strtok(NULL, "&");
    }

    infra_free(params_copy);
    return NULL;
}

infra_error_t poly_db_open(const char* url, poly_db_t** db) {
    if (!url || !db) {
        fprintf(stderr, "poly_db_open: Invalid parameters\n");
        return INFRA_ERROR_INVALID_PARAM;
    }
    *db = NULL;  // 初始化输出参数

    char *scheme = NULL, *path = NULL, *params = NULL;
    infra_error_t err = parse_db_url(url, &scheme, &path, &params);
    if (err != INFRA_OK) {
        // 即使 parse_db_url 部分成功，也要释放可能已分配的内存
        if (scheme) infra_free(scheme);
        if (path) infra_free(path);
        if (params) infra_free(params);
        return err;
    }

    fprintf(stderr, "TMP DEBUG： Opening scheme: %s\n", scheme);
    
    poly_db_t* handle = (poly_db_t*)infra_malloc(sizeof(poly_db_t));
    if (!handle) {
        infra_free(scheme);
        infra_free(path);
        infra_free(params);
        return INFRA_ERROR_NO_MEMORY;
    }
    if (strcmp(scheme, "sqlite") == 0) {
        fprintf(stderr, "SQLite: Returning INFRA_ERROR_NOT_SUPPORTED (%d)\n", INFRA_ERROR_NOT_SUPPORTED);
        // SQLite 动态加载将在后续实现
        infra_free(handle);
        infra_free(scheme);
        infra_free(path);
        infra_free(params);
        return INFRA_ERROR_NOT_SUPPORTED;
    } else if (strcmp(scheme, "duckdb") == 0) {
        fprintf(stderr, "Opening DuckDB database...\n");
        handle->type = POLY_DB_TYPE_DUCKDB;
        
        const char* load_path = "libduckdb.so";
        
        // 加载 DuckDB
        fprintf(stderr, "Loading DuckDB from: %s\n", load_path);
        err = load_duckdb(load_path, &handle->duckdb.funcs);
        if (err != INFRA_OK) {
            fprintf(stderr, "Failed to load DuckDB library\n");
            infra_free(handle);
            infra_free(scheme);
            infra_free(path);
            infra_free(params);
            return err;
        }

        fprintf(stderr, "Opening database: %s\n", path);
        // 如果路径是 ":memory:" 或 NULL，使用内存数据库
        const char* db_path = (path && strcmp(path, ":memory:") != 0) ? path : NULL;

        // 打开数据库
        duckdb_state state = handle->duckdb.funcs->open(db_path, &handle->duckdb.db);
        fprintf(stderr, "DuckDB open state: %d\n", state);
        if (state != DuckDBSuccess) {
            fprintf(stderr, "Failed to open DuckDB database: %s\n", path);
            void* lib_handle = handle->duckdb.funcs->handle;
            duckdb_functions_t* funcs = handle->duckdb.funcs;
            cosmo_dlclose(lib_handle);
            infra_free(funcs);
            infra_free(handle);
            infra_free(scheme);
            infra_free(path);
            infra_free(params);
            return INFRA_ERROR_IO;
        }
        fprintf(stderr, "Database opened successfully\n");

        fprintf(stderr, "Creating connection...\n");
        // 创建连接
        state = handle->duckdb.funcs->connect(handle->duckdb.db, &handle->duckdb.conn);
        fprintf(stderr, "DuckDB connect state: %d\n", state);
        if (state != DuckDBSuccess) {
            fprintf(stderr, "Failed to create DuckDB connection\n");
            handle->duckdb.funcs->close(&handle->duckdb.db);
            void* lib_handle = handle->duckdb.funcs->handle;
            duckdb_functions_t* funcs = handle->duckdb.funcs;
            cosmo_dlclose(lib_handle);
            infra_free(funcs);
            infra_free(handle);
            infra_free(scheme);
            infra_free(path);
            infra_free(params);
            return INFRA_ERROR_IO;
        }
        fprintf(stderr, "DuckDB connection established successfully\n");
    } else {
        infra_free(handle);
        infra_free(scheme);
        infra_free(path);
        infra_free(params);
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_free(scheme);
    infra_free(path);
    infra_free(params);
    *db = handle;
    return INFRA_OK;
}

void poly_db_close(poly_db_t* db) {
    if (!db) return;

    if (db->type == POLY_DB_TYPE_SQLITE) {
        // SQLite 关闭将在后续实现
    } else if (db->type == POLY_DB_TYPE_DUCKDB) {
        if (db->duckdb.funcs) {
            // 先断开连接
            db->duckdb.funcs->disconnect(&db->duckdb.conn);
            // 再关闭数据库
            db->duckdb.funcs->close(&db->duckdb.db);
            // 保存 handle 以便后续关闭
            void* handle = db->duckdb.funcs->handle;
            // 释放函数指针结构体
            infra_free(db->duckdb.funcs);
            // 最后关闭动态库
            cosmo_dlclose(handle);
        }
    }
    infra_free(db);
}

// KV 操作的实现将在后续添加
// 迭代器操作的实现将在后续添加
// SQL 执行操作的实现将在后续添加

// 执行查询
infra_error_t poly_db_query(poly_db_t* db, const char* sql, poly_db_result_t* result) {
    if (!db || !sql || !result) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // 分配结果集结构
    poly_db_result_t res = (poly_db_result_t)infra_malloc(sizeof(struct poly_db_result));
    if (!res) {
        return INFRA_ERROR_NO_MEMORY;
    }
    
    // 初始化结果集
    res->db = db;
    res->internal_result = NULL;
    
    // 根据数据库类型执行查询
    infra_error_t err = INFRA_ERROR_NOT_IMPLEMENTED;
    if (db->type == POLY_DB_TYPE_DUCKDB) {
        duckdb_result* duck_result = (duckdb_result*)infra_malloc(sizeof(duckdb_result));
        if (!duck_result) {
            infra_free(res);
            return INFRA_ERROR_NO_MEMORY;
        }
        
        duckdb_state state = db->duckdb.funcs->query(db->duckdb.conn, sql, duck_result);
        if (state != DuckDBSuccess) {
            infra_free(duck_result);
            infra_free(res);
            return INFRA_ERROR_IO;
        }
        
        res->internal_result = duck_result;
        err = INFRA_OK;
    }
    // TODO: 添加 SQLite 支持
    
    if (err != INFRA_OK) {
        infra_free(res);
        return err;
    }
    
    // 如果调用者已有结果集，先释放它
    if (*result) {
        poly_db_result_free(*result);
    }
    
    *result = res;
    return INFRA_OK;
}

// 获取结果集行数
infra_error_t poly_db_result_row_count(poly_db_result_t result, size_t* count) {
    if (!result || !count) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    poly_db_t* db = ((poly_db_result_t*)result)->db;
    if (!db || !db->interface || !db->interface->result_row_count) {
        return INFRA_ERROR_NOT_IMPLEMENTED;
    }
    
    return db->interface->result_row_count(result, count);
}

// 获取结果集列数
infra_error_t poly_db_result_column_count(poly_db_result_t result, size_t* count) {
    if (!result || !count) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    poly_db_t* db = ((poly_db_result_t*)result)->db;
    if (!db || !db->interface || !db->interface->result_column_count) {
        return INFRA_ERROR_NOT_IMPLEMENTED;
    }
    
    return db->interface->result_column_count(result, count);
}

// 获取字符串值
infra_error_t poly_db_result_get_string(poly_db_result_t result, size_t row, size_t col, char** value) {
    if (!result || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    poly_db_t* db = ((poly_db_result_t*)result)->db;
    if (!db || !db->interface || !db->interface->result_get_string) {
        return INFRA_ERROR_NOT_IMPLEMENTED;
    }
    
    return db->interface->result_get_string(result, row, col, value);
}

// 获取二进制数据
infra_error_t poly_db_result_get_blob(poly_db_result_t result, size_t row, size_t col, void** data, size_t* size) {
    if (!result || !data || !size) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    poly_db_t* db = ((poly_db_result_t*)result)->db;
    if (!db || !db->interface || !db->interface->result_get_blob) {
        return INFRA_ERROR_NOT_IMPLEMENTED;
    }
    
    return db->interface->result_get_blob(result, row, col, data, size);
}

// 释放结果集
void poly_db_result_free(poly_db_result_t result) {
    if (!result) {
        return;
    }
    
    if (result->db) {
        if (result->db->type == POLY_DB_TYPE_DUCKDB && result->internal_result) {
            result->db->duckdb.funcs->destroy_result((duckdb_result*)result->internal_result);
            infra_free(result->internal_result);
        }
        // TODO: 添加 SQLite 支持
    }
    
    infra_free(result);
}
