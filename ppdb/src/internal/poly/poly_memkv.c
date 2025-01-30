#include "internal/poly/poly_memkv.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include <string.h>

//-----------------------------------------------------------------------------
// SQLite implementation
//-----------------------------------------------------------------------------

typedef struct {
    char* path;
} sqlite_impl_t;

//-----------------------------------------------------------------------------
// DuckDB implementation
//-----------------------------------------------------------------------------

typedef struct {
    void* handle;
    char* path;
} duckdb_impl_t;

//-----------------------------------------------------------------------------
// Static functions
//-----------------------------------------------------------------------------

// 初始化存储引擎
static infra_error_t init_engine(poly_memkv_db_t* db, poly_memkv_engine_t engine) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;

    // 根据引擎类型创建具体实现
    switch (engine) {
        case POLY_MEMKV_ENGINE_SQLITE: {
            sqlite_impl_t* sqlite = infra_malloc(sizeof(sqlite_impl_t));
            if (!sqlite) return INFRA_ERROR_NO_MEMORY;
            memset(sqlite, 0, sizeof(sqlite_impl_t));
            db->impl = sqlite;
            break;
        }
        case POLY_MEMKV_ENGINE_DUCKDB: {
            duckdb_impl_t* duckdb = infra_malloc(sizeof(duckdb_impl_t));
            if (!duckdb) return INFRA_ERROR_NO_MEMORY;
            memset(duckdb, 0, sizeof(duckdb_impl_t));
            db->impl = duckdb;
            break;
        }
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }

    return INFRA_OK;
}

// KV操作的内部实现
static infra_error_t kv_set_internal(poly_db_t* db, const char* key, size_t key_len,
                                   const void* value, size_t value_size) {
    if (!db || !key || !value) return INFRA_ERROR_INVALID_PARAM;

    char sql[512];
    snprintf(sql, sizeof(sql), 
        "INSERT OR REPLACE INTO kv_store (key, value) VALUES (?, ?);");

    return poly_db_exec(db, sql);
}

static infra_error_t kv_get_internal(poly_db_t* db, const char* key, size_t key_len,
                                   void** value, size_t* value_size) {
    if (!db || !key || !value || !value_size) return INFRA_ERROR_INVALID_PARAM;

    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT value FROM kv_store WHERE key = ?;");

    poly_db_result_t* result = NULL;
    infra_error_t err = poly_db_query(db, sql, &result);
    if (err != INFRA_OK) return err;

    size_t count = 0;
    err = poly_db_result_row_count(result, &count);
    if (err != INFRA_OK || count == 0) {
        poly_db_result_free(result);
        return INFRA_ERROR_NOT_FOUND;
    }

    err = poly_db_result_get_blob(result, 0, 0, value, value_size);
    poly_db_result_free(result);
    return err;
}

static infra_error_t kv_del_internal(poly_db_t* db, const char* key, size_t key_len) {
    if (!db || !key) return INFRA_ERROR_INVALID_PARAM;

    char sql[512];
    snprintf(sql, sizeof(sql), "DELETE FROM kv_store WHERE key = ?;");

    return poly_db_exec(db, sql);
}

//-----------------------------------------------------------------------------
// Interface Implementation
//-----------------------------------------------------------------------------

// 创建 MemKV 实例
infra_error_t poly_memkv_create(const poly_memkv_config_t* config, poly_memkv_db_t** db) {
    if (!config || !db) return INFRA_ERROR_INVALID_PARAM;

    // 分配内存
    poly_memkv_db_t* memdb = infra_malloc(sizeof(poly_memkv_db_t));
    if (!memdb) return INFRA_ERROR_NO_MEMORY;
    memset(memdb, 0, sizeof(poly_memkv_db_t));

    // 初始化存储引擎
    infra_error_t err = init_engine(memdb, config->engine);
    if (err != INFRA_OK) {
        infra_free(memdb);
        return err;
    }

    // 创建数据库配置
    poly_db_config_t db_config = {
        .type = (config->engine == POLY_MEMKV_ENGINE_SQLITE) ? 
                POLY_DB_TYPE_SQLITE : POLY_DB_TYPE_DUCKDB,
        .url = config->url,
        .max_memory = config->memory_limit,
        .read_only = config->read_only,
        .plugin_path = config->plugin_path,
        .allow_fallback = config->allow_fallback
    };

    // 打开数据库
    err = poly_db_open(&db_config, &memdb->db);
    if (err != INFRA_OK) {
        if (memdb->impl) infra_free(memdb->impl);
        infra_free(memdb);
        return err;
    }

    // 创建 KV 存储表
    const char* create_table_sql = 
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "key TEXT PRIMARY KEY,"
        "value BLOB"
        ");";
    
    err = poly_db_exec(memdb->db, create_table_sql);
    if (err != INFRA_OK) {
        poly_db_close(memdb->db);
        if (memdb->impl) infra_free(memdb->impl);
        infra_free(memdb);
        return err;
    }

    memdb->engine = config->engine;
    memdb->status = POLY_DB_STATUS_OK;
    *db = memdb;
    return INFRA_OK;
}

// 销毁 MemKV 实例
void poly_memkv_destroy(poly_memkv_db_t* db) {
    if (!db) return;

    // 关闭数据库
    if (db->db) {
        poly_db_close(db->db);
    }

    // 清理引擎实现
    if (db->impl) {
        switch (db->engine) {
            case POLY_MEMKV_ENGINE_SQLITE: {
                sqlite_impl_t* sqlite = (sqlite_impl_t*)db->impl;
                if (sqlite->path) infra_free(sqlite->path);
                infra_free(sqlite);
                break;
            }
            case POLY_MEMKV_ENGINE_DUCKDB: {
                duckdb_impl_t* duckdb = (duckdb_impl_t*)db->impl;
                if (duckdb->path) infra_free(duckdb->path);
                if (duckdb->handle) dlclose(duckdb->handle);
                infra_free(duckdb);
                break;
            }
        }
    }

    infra_free(db);
}

// KV 操作实现
infra_error_t poly_memkv_set(poly_memkv_db_t* db, const char* key, const void* value, size_t value_len) {
    if (!db || !key || !value) return INFRA_ERROR_INVALID_PARAM;
    return kv_set_internal(db->db, key, strlen(key), value, value_len);
}

infra_error_t poly_memkv_get(poly_memkv_db_t* db, const char* key, void** value, size_t* value_len) {
    if (!db || !key || !value || !value_len) return INFRA_ERROR_INVALID_PARAM;
    return kv_get_internal(db->db, key, strlen(key), value, value_len);
}

infra_error_t poly_memkv_del(poly_memkv_db_t* db, const char* key) {
    if (!db || !key) return INFRA_ERROR_INVALID_PARAM;
    return kv_del_internal(db->db, key, strlen(key));
}

// 迭代器实现
infra_error_t poly_memkv_iter_create(poly_memkv_db_t* db, poly_memkv_iter_t** iter) {
    if (!db || !iter) return INFRA_ERROR_INVALID_PARAM;

    // 分配迭代器内存
    poly_memkv_iter_t* new_iter = infra_malloc(sizeof(poly_memkv_iter_t));
    if (!new_iter) return INFRA_ERROR_NO_MEMORY;
    memset(new_iter, 0, sizeof(poly_memkv_iter_t));

    // 执行查询
    const char* sql = "SELECT key, value FROM kv_store ORDER BY key;";
    infra_error_t err = poly_db_query(db->db, sql, &new_iter->result);
    if (err != INFRA_OK) {
        infra_free(new_iter);
        return err;
    }

    // 获取总行数
    err = poly_db_result_row_count(new_iter->result, &new_iter->total_rows);
    if (err != INFRA_OK) {
        poly_db_result_free(new_iter->result);
        infra_free(new_iter);
        return err;
    }

    new_iter->engine = db->engine;
    new_iter->current_row = 0;
    *iter = new_iter;
    return INFRA_OK;
}

infra_error_t poly_memkv_iter_next(poly_memkv_iter_t* iter, char** key, void** value, size_t* value_len) {
    if (!iter || !key || !value || !value_len) return INFRA_ERROR_INVALID_PARAM;

    // 检查是否已经遍历完
    if (iter->current_row >= iter->total_rows) {
        return INFRA_ERROR_NOT_FOUND;
    }

    // 获取当前行的数据
    infra_error_t err = poly_db_result_get_string(iter->result, iter->current_row, 0, key);
    if (err != INFRA_OK) return err;

    err = poly_db_result_get_blob(iter->result, iter->current_row, 1, value, value_len);
    if (err != INFRA_OK) {
        infra_free(*key);
        return err;
    }

    iter->current_row++;
    return INFRA_OK;
}

void poly_memkv_iter_destroy(poly_memkv_iter_t* iter) {
    if (!iter) return;

    if (iter->result) {
        poly_db_result_free(iter->result);
    }

    infra_free(iter);
}

// 状态相关函数实现
poly_db_status_t poly_memkv_get_status(const poly_memkv_db_t* db) {
    return db ? db->status : POLY_DB_STATUS_OK;
}

const char* poly_memkv_get_error_message(const poly_memkv_db_t* db) {
    return db ? db->error_msg : "Invalid database handle";
}

bool poly_memkv_is_degraded(const poly_memkv_db_t* db) {
    if (!db) return true;
    return db->engine == POLY_MEMKV_ENGINE_SQLITE && 
           db->status == POLY_DB_STATUS_DEGRADED;
}

// 引擎切换
infra_error_t poly_memkv_switch_engine(poly_memkv_db_t* db,
                                     poly_memkv_engine_t engine_type,
                                     const poly_memkv_config_t* config) {
    if (!db || !config) return INFRA_ERROR_INVALID_PARAM;

    // 创建新的数据库实例
    poly_memkv_db_t* new_db = NULL;
    infra_error_t err = poly_memkv_create(config, &new_db);
    if (err != INFRA_OK) return err;

    // 迁移数据
    poly_memkv_iter_t* iter = NULL;
    err = poly_memkv_iter_create(db, &iter);
    if (err != INFRA_OK) {
        poly_memkv_destroy(new_db);
        return err;
    }

    char* key;
    void* value;
    size_t value_len;
    while (poly_memkv_iter_next(iter, &key, &value, &value_len) == INFRA_OK) {
        err = poly_memkv_set(new_db, key, value, value_len);
        infra_free(key);
        infra_free(value);
        if (err != INFRA_OK) {
            poly_memkv_iter_destroy(iter);
            poly_memkv_destroy(new_db);
            return err;
        }
    }

    poly_memkv_iter_destroy(iter);

    // 保存旧的实现和数据库
    void* old_impl = db->impl;
    poly_db_t* old_db = db->db;

    // 切换到新的实现
    db->impl = new_db->impl;
    db->db = new_db->db;
    db->engine = new_db->engine;
    new_db->impl = old_impl;
    new_db->db = old_db;

    poly_memkv_destroy(new_db);
    return INFRA_OK;
}
