#include "internal/poly/poly_memkv.h"
#include "internal/infra/infra_core.h"
#include "internal/poly/poly_sqlite.h"
#include "internal/poly/poly_duckdb.h"

// MemKV 实例结构体
struct poly_memkv {
    poly_memkv_config_t config;
    poly_memkv_stats_t stats;
    void* plugin_mgr;
    void* engine;
};

//-----------------------------------------------------------------------------
// Static functions
//-----------------------------------------------------------------------------

// 初始化 SQLite 引擎
static infra_error_t init_sqlite_engine(poly_memkv_t* store) {
    poly_builtin_plugin_t plugin = {
        .type = POLY_PLUGIN_SQLITE,
        .name = "sqlite",
        .interface = &g_sqlite_interface
    };

    infra_error_t err = poly_plugin_register_builtin(store->plugin_mgr, &plugin);
    if (err != INFRA_OK) {
        return err;
    }

    err = poly_plugin_mgr_get(store->plugin_mgr, POLY_PLUGIN_SQLITE, "sqlite", &store->engine_plugin);
    if (err != INFRA_OK) {
        return err;
    }

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    err = interface->init(&store->engine_handle);
    if (err != INFRA_OK) {
        return err;
    }

    return INFRA_OK;
}

// 初始化 DuckDB 引擎
static infra_error_t init_duckdb_engine(poly_memkv_t* store) {
    infra_error_t err = poly_plugin_mgr_load(store->plugin_mgr, POLY_PLUGIN_DUCKDB, store->config.path, &store->engine_plugin);
    if (err != INFRA_OK) {
        return err;
    }

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    err = interface->init(&store->engine_handle);
    if (err != INFRA_OK) {
        return err;
    }

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Public functions
//-----------------------------------------------------------------------------

// 创建 MemKV 实例
infra_error_t poly_memkv_create(poly_memkv_t** store) {
    if (!store) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_memkv_t* new_store = (poly_memkv_t*)infra_malloc(sizeof(poly_memkv_t));
    if (!new_store) {
        return INFRA_ERROR_NO_MEMORY;
    }

    memset(new_store, 0, sizeof(poly_memkv_t));

    // 初始化统计信息
    poly_atomic_init(&new_store->stats.cmd_get, 0);
    poly_atomic_init(&new_store->stats.cmd_set, 0);
    poly_atomic_init(&new_store->stats.cmd_del, 0);
    poly_atomic_init(&new_store->stats.curr_items, 0);
    poly_atomic_init(&new_store->stats.hits, 0);
    poly_atomic_init(&new_store->stats.misses, 0);

    // 创建插件管理器
    infra_error_t err = poly_plugin_mgr_create(&new_store->plugin_mgr);
    if (err != INFRA_OK) {
        infra_free(new_store);
        return err;
    }

    *store = new_store;
    return INFRA_OK;
}

// 配置 MemKV 实例
infra_error_t poly_memkv_configure(poly_memkv_t* store, const poly_memkv_config_t* config) {
    if (!store || !config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 复制配置
    store->config = *config;

    // 初始化存储引擎
    infra_error_t err;
    if (config->engine_type == POLY_MEMKV_ENGINE_SQLITE) {
        err = init_sqlite_engine(store);
    } else {
        err = init_duckdb_engine(store);
    }

    return err;
}

// 打开存储
infra_error_t poly_memkv_open(poly_memkv_t* store) {
    if (!store || !store->engine_plugin) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    return interface->open(store->engine_handle, store->config.path);
}

// 关闭存储
void poly_memkv_close(poly_memkv_t* store) {
    if (!store || !store->engine_plugin) {
        return;
    }

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    interface->close(store->engine_handle);
}

// 销毁 MemKV 实例
void poly_memkv_destroy(poly_memkv_t* store) {
    if (!store) {
        return;
    }

    if (store->engine_plugin) {
        const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
        interface->cleanup(store->engine_handle);
    }

    if (store->plugin_mgr) {
        poly_plugin_mgr_destroy(store->plugin_mgr);
    }

    infra_free(store);
}

// 设置键值对
infra_error_t poly_memkv_set(poly_memkv_t* store, const char* key, size_t key_len,
    const void* value, size_t value_len) {
    if (!store || !key || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (key_len > store->config.max_key_size || value_len > store->config.max_value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    infra_error_t err = interface->set(store->engine_handle, key, key_len, value, value_len);
    if (err == INFRA_OK) {
        poly_atomic_inc(&store->stats.cmd_set);
        poly_atomic_inc(&store->stats.curr_items);
    }

    return err;
}

// 获取键值对
infra_error_t poly_memkv_get(poly_memkv_t* store, const char* key, size_t key_len,
    void** value, size_t* value_len) {
    if (!store || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (key_len > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_atomic_inc(&store->stats.cmd_get);

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    infra_error_t err = interface->get(store->engine_handle, key, key_len, value, value_len);
    if (err == INFRA_OK) {
        poly_atomic_inc(&store->stats.hits);
    } else {
        poly_atomic_inc(&store->stats.misses);
    }

    return err;
}

// 删除键值对
infra_error_t poly_memkv_del(poly_memkv_t* store, const char* key, size_t key_len) {
    if (!store || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (key_len > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    infra_error_t err = interface->del(store->engine_handle, key, key_len);
    if (err == INFRA_OK) {
        poly_atomic_inc(&store->stats.cmd_del);
        poly_atomic_dec(&store->stats.curr_items);
    }

    return err;
}

// 创建迭代器
infra_error_t poly_memkv_iter_create(poly_memkv_t* store, poly_memkv_iter_t** iter) {
    if (!store || !iter) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_memkv_iter_t* new_iter = (poly_memkv_iter_t*)infra_malloc(sizeof(poly_memkv_iter_t));
    if (!new_iter) {
        return INFRA_ERROR_NO_MEMORY;
    }

    new_iter->store = store;

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    infra_error_t err = interface->iter_create(store->engine_handle, &new_iter->engine_iter);
    if (err != INFRA_OK) {
        infra_free(new_iter);
        return err;
    }

    *iter = new_iter;
    return INFRA_OK;
}

// 迭代下一个键值对
infra_error_t poly_memkv_iter_next(poly_memkv_iter_t* iter, char** key, size_t* key_len,
    void** value, size_t* value_len) {
    if (!iter || !key || !key_len || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(iter->store->engine_plugin);
    return interface->iter_next(iter->engine_iter, key, key_len, value, value_len);
}

// 销毁迭代器
void poly_memkv_iter_destroy(poly_memkv_iter_t* iter) {
    if (!iter) {
        return;
    }

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(iter->store->engine_plugin);
    interface->iter_destroy(iter->engine_iter);
    infra_free(iter);
}

// 获取存储引擎类型
poly_memkv_engine_type_t poly_memkv_get_engine_type(const poly_memkv_t* store) {
    return store ? store->config.engine_type : POLY_MEMKV_ENGINE_SQLITE;
}

// 获取统计信息
const poly_memkv_stats_t* poly_memkv_get_stats(poly_memkv_t* store) {
    if (!store) {
        return NULL;
    }
    return &store->stats;
}

// 切换存储引擎
infra_error_t poly_memkv_switch_engine(poly_memkv_t* store,
                                     poly_memkv_engine_type_t engine_type,
                                     const poly_memkv_config_t* config) {
    if (!store) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 如果引擎类型相同且没有新配置，无需切换
    if (store->config.engine_type == engine_type && !config) {
        return INFRA_OK;
    }

    // 保存当前配置
    poly_memkv_config_t old_config = store->config;
    
    // 如果提供了新配置，更新配置
    if (config) {
        store->config = *config;
    }
    
    // 切换引擎
    infra_error_t err = poly_memkv_init_engine(store, engine_type);
    if (err != INFRA_OK) {
        // 恢复旧配置
        store->config = old_config;
        return err;
    }

    return INFRA_OK;
}

// ... 其他函数的实现 ...