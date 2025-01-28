#include "internal/infra/infra_core.h"
#include "internal/poly/poly_memkv.h"
#include "internal/poly/poly_db.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_memory.h"

//-----------------------------------------------------------------------------
// Static functions
//-----------------------------------------------------------------------------

// 初始化存储引擎
static infra_error_t init_engine(poly_memkv_t* store, poly_memkv_engine_t engine) {
    if (!store) return INFRA_ERROR_INVALID_PARAM;

    // 注册插件
    poly_builtin_plugin_t plugin = {
        .name = (engine == POLY_MEMKV_ENGINE_SQLITE) ? "sqlite" : "duckdb",
        .type = (engine == POLY_MEMKV_ENGINE_SQLITE) ? POLY_PLUGIN_SQLITE : POLY_PLUGIN_DUCKDB,
        .interface = NULL  // 由插件系统自动加载
    };

    infra_error_t err = poly_plugin_register_builtin(store->plugin_mgr, &plugin);
    if (err != INFRA_OK) return err;

    // 获取插件实例
    return poly_plugin_mgr_get(store->plugin_mgr, plugin.type, plugin.name, &store->engine_plugin);
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
// Public functions
//-----------------------------------------------------------------------------

// 创建 MemKV 实例
infra_error_t poly_memkv_create(const poly_memkv_config_t* config, poly_memkv_db_t** db) {
    if (!config || !db) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Allocate memory for the handle
    poly_memkv_db_t* memkv = infra_malloc(sizeof(poly_memkv_db_t));
    if (!memkv) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(memkv, 0, sizeof(poly_memkv_db_t));

    // Initialize based on engine type
    infra_error_t err = INFRA_OK;
    
    // 构造数据库 URL
    char url[1024];
    switch (config->engine) {
        case POLY_MEMKV_ENGINE_SQLITE:
            snprintf(url, sizeof(url), "sqlite://%s", config->path);
            break;
        case POLY_MEMKV_ENGINE_DUCKDB:
            snprintf(url, sizeof(url), "duckdb://%s", config->path);
            break;
        default:
            infra_free(memkv);
            return INFRA_ERROR_INVALID_PARAM;
    }

    // 使用 poly_db 打开数据库
    err = poly_db_open(url, &memkv->db);
    if (err != INFRA_OK) {
        infra_free(memkv);
        return err;
    }

    memkv->engine = config->engine;
    *db = memkv;
    return INFRA_OK;
}

// 销毁 MemKV 实例
void poly_memkv_destroy(poly_memkv_db_t* db) {
    if (!db) return;

    if (db->db) {
        poly_db_close(db->db);
    }
    infra_free(db);
}

// 设置键值对
infra_error_t poly_memkv_set(poly_memkv_db_t* db, const char* key, const void* value, size_t value_len) {
    if (!db || !key || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    return kv_set_internal(db->db, key, strlen(key), value, value_len);
}

// 获取键值对
infra_error_t poly_memkv_get(poly_memkv_db_t* db, const char* key, void** value, size_t* value_len) {
    if (!db || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    return kv_get_internal(db->db, key, strlen(key), value, value_len);
}

// 删除键值对
infra_error_t poly_memkv_del(poly_memkv_db_t* db, const char* key) {
    if (!db || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    return kv_del_internal(db->db, key, strlen(key));
}

// 执行 SQL
infra_error_t poly_memkv_exec(poly_memkv_db_t* db, const char* sql) {
    if (!db || !sql) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    return poly_db_exec(db->db, sql);
}

// 配置 MemKV 实例
infra_error_t poly_memkv_configure(poly_memkv_t* store, const poly_memkv_config_t* config) {
    if (!store || !config) return INFRA_ERROR_INVALID_PARAM;

    // 复制配置
    store->config = *config;
    if (config->path) {
        store->config.path = infra_strdup(config->path);
        if (!store->config.path) return INFRA_ERROR_NO_MEMORY;
    }

    return INFRA_OK;
}

// 打开存储
infra_error_t poly_memkv_open(poly_memkv_t* store) {
    if (!store) return INFRA_ERROR_INVALID_PARAM;

    // 初始化存储引擎
    infra_error_t err;
    if (store->config.engine_type == POLY_MEMKV_ENGINE_SQLITE) {
        err = init_engine(store, POLY_MEMKV_ENGINE_SQLITE);
    } else {
        err = init_engine(store, POLY_MEMKV_ENGINE_DUCKDB);
    }
    if (err != INFRA_OK) return err;

    // 获取插件接口
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    if (!interface) return INFRA_ERROR_INVALID_STATE;

    // 初始化引擎
    err = interface->init(&store->engine_handle);
    if (err != INFRA_OK) return err;

    // 根据引擎类型打开数据库
    if (store->config.engine_type == POLY_MEMKV_ENGINE_SQLITE) {
        const poly_sqlitekv_interface_t* sqlite_interface = (const poly_sqlitekv_interface_t*)interface;
        err = sqlite_interface->open(store->engine_handle, store->config.path);
    } else {
        const poly_duckdbkv_interface_t* duckdb_interface = (const poly_duckdbkv_interface_t*)interface;
        err = duckdb_interface->open(&store->engine_handle, store->config.path);
    }
    if (err != INFRA_OK) {
        interface->cleanup(store->engine_handle);
        return err;
    }

    return INFRA_OK;
}

// 关闭存储
void poly_memkv_close(poly_memkv_t* store) {
    if (!store) return;

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    if (interface) {
        interface->cleanup(store->engine_handle);
    }
}

// 获取存储引擎类型
poly_memkv_engine_type_t poly_memkv_get_engine_type(const poly_memkv_t* store) {
    return store ? store->config.engine_type : POLY_MEMKV_ENGINE_SQLITE;
}

// 获取统计信息
const poly_memkv_stats_t* poly_memkv_get_stats(poly_memkv_t* store) {
    return store ? &store->stats : NULL;
}

// 切换存储引擎
infra_error_t poly_memkv_switch_engine(poly_memkv_t* store,
                                     poly_memkv_engine_type_t engine_type,
                                     const poly_memkv_config_t* config) {
    if (!store || !config) return INFRA_ERROR_INVALID_PARAM;

    // 关闭当前存储
    poly_memkv_close(store);

    // 清理旧的插件管理器
    if (store->plugin_mgr) {
        poly_plugin_mgr_destroy(store->plugin_mgr);
        store->plugin_mgr = NULL;
    }

    // 创建新的插件管理器
    infra_error_t err = poly_plugin_mgr_create(&store->plugin_mgr);
    if (err != INFRA_OK) return err;

    // 更新配置
    err = poly_memkv_configure(store, config);
    if (err != INFRA_OK) return err;

    // 重新打开存储
    return poly_memkv_open(store);
}

// 打开数据库
infra_error_t poly_memkv_open(poly_memkv_db_t** db, const char* path, poly_memkv_engine_t engine) {
    if (!db || !path) return INFRA_ERROR_INVALID_PARAM;

    poly_memkv_db_t* memdb = infra_malloc(sizeof(poly_memkv_db_t));
    if (!memdb) return INFRA_ERROR_NO_MEMORY;
    memset(memdb, 0, sizeof(poly_memkv_db_t));
    memdb->engine = engine;

    // 构造数据库 URL
    char url[1024];
    switch (engine) {
        case POLY_MEMKV_ENGINE_SQLITE:
            snprintf(url, sizeof(url), "sqlite://%s", path);
            break;
        case POLY_MEMKV_ENGINE_DUCKDB:
            snprintf(url, sizeof(url), "duckdb://%s", path);
            break;
        default:
            infra_free(memdb);
            return INFRA_ERROR_INVALID_PARAM;
    }

    // 使用 poly_db 打开数据库
    infra_error_t err = poly_db_open(url, &memdb->db);
    if (err != INFRA_OK) {
        infra_free(memdb);
        return err;
    }

    // 根据引擎类型创建具体实现
    switch (engine) {
        case POLY_MEMKV_ENGINE_SQLITE:
            err = poly_sqlitekv_open((poly_sqlitekv_db_t**)&memdb->impl, path);
            break;
        case POLY_MEMKV_ENGINE_DUCKDB:
            err = poly_duckdbkv_open((poly_duckdbkv_db_t**)&memdb->impl, path);
            break;
    }

    if (err != INFRA_OK) {
        poly_db_close(memdb->db);
        infra_free(memdb);
        return err;
    }

    *db = memdb;
    return INFRA_OK;
}

// 关闭数据库
void poly_memkv_close(poly_memkv_db_t* db) {
    if (!db) return;

    // 关闭具体实现
    switch (db->engine) {
        case POLY_MEMKV_ENGINE_SQLITE:
            poly_sqlitekv_close((poly_sqlitekv_db_t*)db->impl);
            break;
        case POLY_MEMKV_ENGINE_DUCKDB:
            poly_duckdbkv_close((poly_duckdbkv_db_t*)db->impl);
            break;
    }

    // 关闭底层数据库
    if (db->db) poly_db_close(db->db);
    infra_free(db);
}

