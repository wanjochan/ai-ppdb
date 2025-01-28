#include "internal/poly/poly_db.h"

// #include "sqlite3.h" 既然是 dlxxxx 加载，就不用跟头文件，而是参考
//https://duckdb.org/docs/api/c/api.html

// DuckDB 函数指针类型定义
typedef int (*duckdb_open_t)(const char*, void**);
typedef void (*duckdb_close_t)(void*);
typedef int (*duckdb_connect_t)(void*, void**);
typedef void (*duckdb_disconnect_t)(void*);
typedef int (*duckdb_query_t)(void*, const char*, void**);
typedef void (*duckdb_destroy_result_t)(void*);

// DuckDB 动态加载结构
typedef struct {
    void* handle;
    duckdb_open_t open;
    duckdb_close_t close;
    duckdb_connect_t connect;
    duckdb_disconnect_t disconnect;
    duckdb_query_t query;
    duckdb_destroy_result_t destroy_result;
} duckdb_functions_t;

// 数据库句柄结构
struct poly_db {
    poly_db_type_t type;
    union {
        poly_db_handle_t sqlite;
        struct {
            poly_db_handle_t db;
            poly_db_connection_t conn;
            duckdb_functions_t* funcs;
        } duckdb;
    };
};

// 迭代器结构
struct poly_db_iter {
    poly_db_t* db;
    union {
        poly_db_stmt_t sqlite_stmt;
        poly_db_result_t duckdb_result;
    };
    size_t current_row;
};

// URL 解析函数 （成熟后放 poly_db.h 中可以更多地方用到）
static infra_error_t parse_db_url(const char* url, char** scheme, char** path, char** params) {
    if (!url || !scheme || !path || !params) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* scheme_end = strstr(url, "://");
    if (!scheme_end) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t scheme_len = scheme_end - url;
    *scheme = (char*)malloc(scheme_len + 1);
    if (!*scheme) {
        return INFRA_ERROR_NO_MEMORY;
    }
    strncpy(*scheme, url, scheme_len);
    (*scheme)[scheme_len] = '\0';

    const char* path_start = scheme_end + 3;
    const char* params_start = strchr(path_start, '?');
    
    if (params_start) {
        size_t path_len = params_start - path_start;
        *path = (char*)malloc(path_len + 1);
        if (!*path) {
            free(*scheme);
            return INFRA_ERROR_NO_MEMORY;
        }
        strncpy(*path, path_start, path_len);
        (*path)[path_len] = '\0';

        size_t params_len = strlen(params_start + 1);
        *params = (char*)malloc(params_len + 1);
        if (!*params) {
            free(*scheme);
            free(*path);
            return INFRA_ERROR_NO_MEMORY;
        }
        strcpy(*params, params_start + 1);
    } else {
        *path = strdup(path_start);
        if (!*path) {
            free(*scheme);
            return INFRA_ERROR_NO_MEMORY;
        }
        *params = strdup("");
        if (!*params) {
            free(*scheme);
            free(*path);
            return INFRA_ERROR_NO_MEMORY;
        }
    }

    return INFRA_OK;
}

// DuckDB 动态加载
static infra_error_t load_duckdb(const char* lib_path, duckdb_functions_t** funcs) {
    duckdb_functions_t* f = malloc(sizeof(duckdb_functions_t));
    if (!f) return INFRA_ERROR_NO_MEMORY;

    f->handle = cosmo_dlopen(lib_path, RTLD_LAZY);
    if (!f->handle) {
        free(f);
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 加载所需的函数
    f->open = (duckdb_open_t)cosmo_dlsym(f->handle, "duckdb_open");
    f->close = (duckdb_close_t)cosmo_dlsym(f->handle, "duckdb_close");
    f->connect = (duckdb_connect_t)cosmo_dlsym(f->handle, "duckdb_connect");
    f->disconnect = (duckdb_disconnect_t)cosmo_dlsym(f->handle, "duckdb_disconnect");
    f->query = (duckdb_query_t)cosmo_dlsym(f->handle, "duckdb_query");
    f->destroy_result = (duckdb_destroy_result_t)cosmo_dlsym(f->handle, "duckdb_destroy_result");

    if (!f->open || !f->close || !f->connect || !f->disconnect || !f->query || !f->destroy_result) {
        cosmo_dlclose(f->handle);
        free(f);
        return INFRA_ERROR_INVALID_PARAM;
    }

    *funcs = f;
    return INFRA_OK;
}

// 获取参数值
static char* get_param_value(const char* params, const char* key) {
    char* params_copy = strdup(params);
    if (!params_copy) return NULL;

    char* param = strtok(params_copy, "&");
    while (param) {
        char* equals = strchr(param, '=');
        if (equals) {
            *equals = '\0';
            if (strcmp(param, key) == 0) {
                char* value = strdup(equals + 1);
                free(params_copy);
                return value;
            }
        }
        param = strtok(NULL, "&");
    }

    free(params_copy);
    return NULL;
}

infra_error_t poly_db_open(const char* url, poly_db_t** db) {
    char *scheme, *path, *params;
    infra_error_t err = parse_db_url(url, &scheme, &path, &params);
    if (err != INFRA_OK) return err;

    poly_db_t* handle = malloc(sizeof(poly_db_t));
    if (!handle) {
        free(scheme);
        free(path);
        free(params);
        return INFRA_ERROR_NO_MEMORY;
    }

    if (strcmp(scheme, "sqlite") == 0) {
        handle->type = POLY_DB_TYPE_SQLITE;
        // SQLite 动态加载将在后续实现
        handle->sqlite = NULL;
        err = INFRA_OK;
    } else if (strcmp(scheme, "duckdb") == 0) {
        handle->type = POLY_DB_TYPE_DUCKDB;
        
        // 获取 load_path 参数
        char* load_path = get_param_value(params, "load_path");
        if (!load_path) {
            load_path = "libduckdb.so";//cosmo_dlopen will auto find .dll/.dylib
            // free(handle);
            // free(scheme);
            // free(path);
            // free(params);
            // return INFRA_ERROR_INVALID_PARAM;
        }
        
        // 加载 DuckDB
        err = load_duckdb(load_path, &handle->duckdb.funcs);
        free(load_path);
        if (err != INFRA_OK) {
            free(handle);
            free(scheme);
            free(path);
            free(params);
            return err;
        }

        // 打开数据库
        if (handle->duckdb.funcs->open(path, &handle->duckdb.db) != 0) {
            cosmo_dlclose(handle->duckdb.funcs->handle);
            free(handle->duckdb.funcs);
            free(handle);
            free(scheme);
            free(path);
            free(params);
            return INFRA_ERROR_INVALID_PARAM;
        }

        // 创建连接
        if (handle->duckdb.funcs->connect(handle->duckdb.db, &handle->duckdb.conn) != 0) {
            handle->duckdb.funcs->close(handle->duckdb.db);
            cosmo_dlclose(handle->duckdb.funcs->handle);
            free(handle->duckdb.funcs);
            free(handle);
            free(scheme);
            free(path);
            free(params);
            return INFRA_ERROR_INVALID_PARAM;
        }
    } else {
        free(handle);
        free(scheme);
        free(path);
        free(params);
        return INFRA_ERROR_INVALID_PARAM;
    }

    free(scheme);
    free(path);
    free(params);
    *db = handle;
    return INFRA_OK;
}

void poly_db_close(poly_db_t* db) {
    if (!db) return;

    if (db->type == POLY_DB_TYPE_SQLITE) {
        // SQLite 关闭将在后续实现
    } else if (db->type == POLY_DB_TYPE_DUCKDB) {
        db->duckdb.funcs->disconnect(db->duckdb.conn);
        db->duckdb.funcs->close(db->duckdb.db);
        cosmo_dlclose(db->duckdb.funcs->handle);
        free(db->duckdb.funcs);
    }
    free(db);
}

// KV 操作的实现将在后续添加
// 迭代器操作的实现将在后续添加
// SQL 执行操作的实现将在后续添加
