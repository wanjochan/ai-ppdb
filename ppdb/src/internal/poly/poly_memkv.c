#include "internal/poly/poly_memkv.h"
#include "internal/poly/poly_sqlite.h"
#include "internal/poly/poly_duckdb.h"

//-----------------------------------------------------------------------------
// Static functions
//-----------------------------------------------------------------------------

// 初始化SQLite引擎
static infra_error_t init_sqlite_engine(poly_memkv_t* store) {
    // 注册SQLite插件
    poly_builtin_plugin_t plugin = {
        .name = "sqlite",
        .type = POLY_PLUGIN_SQLITE,
        .interface = poly_sqlite_get_interface()
    };
    infra_error_t err = poly_plugin_register_builtin(store->plugins, &plugin);
    if (err != INFRA_OK) {
        return err;
    }

    // 获取SQLite插件
    err = poly_plugin_mgr_get(store->plugins, POLY_PLUGIN_SQLITE, "sqlite", &store->engine_plugin);
    if (err != INFRA_OK) {
        return err;
    }

    // 初始化SQLite引擎
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    err = interface->init(&store->engine_handle);
    if (err != INFRA_OK) {
        return err;
    }

    store->type = POLY_MEMKV_ENGINE_SQLITE;
    return INFRA_OK;
}

// 初始化DuckDB引擎
static infra_error_t init_duckdb_engine(poly_memkv_t* store, const char* plugin_path) {
    // 加载DuckDB插件
    infra_error_t err = poly_plugin_mgr_load(store->plugins, POLY_PLUGIN_DUCKDB, plugin_path, &store->engine_plugin);
    if (err != INFRA_OK) {
        return err;
    }

    // 初始化DuckDB引擎
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    err = interface->init(&store->engine_handle);
    if (err != INFRA_OK) {
        return err;
    }

    store->type = POLY_MEMKV_ENGINE_DUCKDB;
    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Public functions
//-----------------------------------------------------------------------------

infra_error_t poly_memkv_create(const poly_memkv_config_t* config, poly_memkv_t** store) {
    if (!config || !store) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 分配内存
    poly_memkv_t* new_store = (poly_memkv_t*)malloc(sizeof(poly_memkv_t));
    if (!new_store) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(new_store, 0, sizeof(poly_memkv_t));

    // 复制配置
    new_store->config = *config;

    // 创建插件管理器
    infra_error_t err = poly_plugin_mgr_create(&new_store->plugins);
    if (err != INFRA_OK) {
        free(new_store);
        return err;
    }

    // 初始化存储引擎
    if (config->engine_type == POLY_MEMKV_ENGINE_DUCKDB) {
        err = init_duckdb_engine(new_store, config->plugin_path);
    } else {
        err = init_sqlite_engine(new_store);
    }
    if (err != INFRA_OK) {
        poly_plugin_mgr_destroy(new_store->plugins);
        free(new_store);
        return err;
    }

    *store = new_store;
    return INFRA_OK;
}

void poly_memkv_destroy(poly_memkv_t* store) {
    if (!store) {
        return;
    }

    // 清理存储引擎
    if (store->engine_plugin) {
        const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
        interface->cleanup(store->engine_handle);
    }

    // 清理插件管理器
    if (store->plugins) {
        poly_plugin_mgr_destroy(store->plugins);
    }

    free(store);
}

infra_error_t poly_memkv_set(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size) {
    if (!store || !key || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 检查键值大小限制
    if (strlen(key) > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (value_size > store->config.max_value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 调用存储引擎接口
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    infra_error_t err = interface->set(store->engine_handle, key, value, value_size);
    if (err == INFRA_OK) {
        poly_atomic_inc(&store->stats.cmd_set);
        poly_atomic_inc(&store->stats.curr_items);
        poly_atomic_inc(&store->stats.total_items);
        poly_atomic_add(&store->stats.bytes, value_size);
    }
    return err;
}

infra_error_t poly_memkv_get(poly_memkv_t* store, const char* key,
    void** value, size_t* value_size) {
    if (!store || !key || !value || !value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 检查键大小限制
    if (strlen(key) > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 调用存储引擎接口
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    infra_error_t err = interface->get(store->engine_handle, key, value, value_size);
    if (err == INFRA_OK) {
        poly_atomic_inc(&store->stats.cmd_get);
        poly_atomic_inc(&store->stats.hits);
    } else {
        poly_atomic_inc(&store->stats.misses);
    }
    return err;
}

infra_error_t poly_memkv_del(poly_memkv_t* store, const char* key) {
    if (!store || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 检查键大小限制
    if (strlen(key) > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 调用存储引擎接口
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    infra_error_t err = interface->del(store->engine_handle, key);
    if (err == INFRA_OK) {
        poly_atomic_dec(&store->stats.curr_items);
    }
    return err;
}

const poly_memkv_stats_t* poly_memkv_get_stats(poly_memkv_t* store) {
    if (!store) {
        return NULL;
    }
    return &store->stats;
}

infra_error_t poly_memkv_switch_engine(poly_memkv_t* store, 
    poly_memkv_engine_type_t type, const char* plugin_path) {
    if (!store) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 如果类型相同，无需切换
    if (store->type == type) {
        return INFRA_OK;
    }

    // 清理当前引擎
    if (store->engine_plugin) {
        const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
        interface->cleanup(store->engine_handle);
        store->engine_plugin = NULL;
        store->engine_handle = NULL;
    }

    // 初始化新引擎
    infra_error_t err;
    if (type == POLY_MEMKV_ENGINE_DUCKDB) {
        err = init_duckdb_engine(store, plugin_path);
    } else {
        err = init_sqlite_engine(store);
    }

    return err;
}

poly_memkv_engine_type_t poly_memkv_get_engine_type(const poly_memkv_t* store) {
    return store ? store->type : POLY_MEMKV_ENGINE_SQLITE;
}

// ... 其他函数的实现 ...