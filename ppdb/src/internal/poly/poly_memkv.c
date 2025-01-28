#include "internal/poly/poly_memkv.h"
#include "internal/infra/infra_core.h"
#include "internal/poly/poly_sqlite.h"
#include "internal/poly/poly_duckdb.h"

//-----------------------------------------------------------------------------
// Static functions
//-----------------------------------------------------------------------------

// 初始化 SQLite 引擎
static infra_error_t init_sqlite_engine(poly_memkv_t* store) {
    if (!store) return INFRA_ERROR_INVALID_PARAM;

    // 注册 SQLite 插件
    poly_builtin_plugin_t plugin = {
        .name = "sqlite",
        .type = POLY_PLUGIN_SQLITE,
        .interface = (const poly_plugin_interface_t*)&g_sqlite_interface
    };

    infra_error_t err = poly_plugin_register_builtin(store->plugin_mgr, &plugin);
    if (err != INFRA_OK) return err;

    // 获取插件实例
    return poly_plugin_mgr_get(store->plugin_mgr, POLY_PLUGIN_SQLITE, "sqlite", &store->engine_plugin);
}

// 初始化 DuckDB 引擎
static infra_error_t init_duckdb_engine(poly_memkv_t* store) {
    if (!store) return INFRA_ERROR_INVALID_PARAM;

    // 注册 DuckDB 插件
    poly_builtin_plugin_t plugin = {
        .name = "duckdb",
        .type = POLY_PLUGIN_DUCKDB,
        .interface = (const poly_plugin_interface_t*)&g_duckdb_interface
    };

    infra_error_t err = poly_plugin_register_builtin(store->plugin_mgr, &plugin);
    if (err != INFRA_OK) return err;

    // 获取插件实例
    return poly_plugin_mgr_get(store->plugin_mgr, POLY_PLUGIN_DUCKDB, "duckdb", &store->engine_plugin);
}

//-----------------------------------------------------------------------------
// Public functions
//-----------------------------------------------------------------------------

// 创建 MemKV 实例
infra_error_t poly_memkv_create(poly_memkv_t** store) {
    if (!store) return INFRA_ERROR_INVALID_PARAM;

    // 分配内存
    poly_memkv_t* new_store = infra_malloc(sizeof(poly_memkv_t));
    if (!new_store) return INFRA_ERROR_NO_MEMORY;

    // 初始化成员
    memset(new_store, 0, sizeof(poly_memkv_t));
    new_store->config.engine_type = POLY_MEMKV_ENGINE_SQLITE;  // 默认使用 SQLite

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
        err = init_sqlite_engine(store);
    } else {
        err = init_duckdb_engine(store);
    }
    if (err != INFRA_OK) return err;

    // 获取插件接口
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    if (!interface) return INFRA_ERROR_INVALID_STATE;

    // 初始化引擎
    err = interface->init(&store->engine_handle);
    if (err != INFRA_OK) return err;

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

// 销毁 MemKV 实例
void poly_memkv_destroy(poly_memkv_t* store) {
    if (!store) return;

    // 关闭存储
    poly_memkv_close(store);

    // 清理资源
    if (store->config.path) {
        infra_free(store->config.path);
    }
    if (store->plugin_mgr) {
        poly_plugin_mgr_destroy(store->plugin_mgr);
    }
    infra_free(store);
}

// 设置键值对
infra_error_t poly_memkv_set(poly_memkv_t* store, const char* key, size_t key_len,
                            const void* value, size_t value_len) {
    if (!store || !key || !value) return INFRA_ERROR_INVALID_PARAM;

    // 检查长度限制
    if (store->config.max_key_size > 0 && key_len > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (store->config.max_value_size > 0 && value_len > store->config.max_value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 获取插件接口
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    if (!interface) return INFRA_ERROR_INVALID_STATE;

    // 设置键值对
    infra_error_t err = interface->set(store->engine_handle, key, value, value_len);
    if (err == INFRA_OK) {
        poly_atomic_inc(&store->stats.cmd_set);
    }

    return err;
}

// 获取键值对
infra_error_t poly_memkv_get(poly_memkv_t* store, const char* key, size_t key_len,
                            void** value, size_t* value_len) {
    if (!store || !key || !value || !value_len) return INFRA_ERROR_INVALID_PARAM;

    // 检查长度限制
    if (store->config.max_key_size > 0 && key_len > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 获取插件接口
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    if (!interface) return INFRA_ERROR_INVALID_STATE;

    // 获取键值对
    infra_error_t err = interface->get(store->engine_handle, key, value, value_len);
    if (err == INFRA_OK) {
        poly_atomic_inc(&store->stats.cmd_get);
        poly_atomic_inc(&store->stats.hits);
    } else if (err == INFRA_ERROR_NOT_FOUND) {
        poly_atomic_inc(&store->stats.cmd_get);
        poly_atomic_inc(&store->stats.misses);
    }

    return err;
}

// 删除键值对
infra_error_t poly_memkv_del(poly_memkv_t* store, const char* key, size_t key_len) {
    if (!store || !key) return INFRA_ERROR_INVALID_PARAM;

    // 检查长度限制
    if (store->config.max_key_size > 0 && key_len > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 获取插件接口
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    if (!interface) return INFRA_ERROR_INVALID_STATE;

    // 删除键值对
    infra_error_t err = interface->del(store->engine_handle, key);
    if (err == INFRA_OK) {
        poly_atomic_inc(&store->stats.cmd_del);
    }

    return err;
}

// 创建迭代器
infra_error_t poly_memkv_iter_create(poly_memkv_t* store, poly_memkv_iter_t** iter) {
    if (!store || !iter) return INFRA_ERROR_INVALID_PARAM;

    // 分配内存
    poly_memkv_iter_t* new_iter = infra_malloc(sizeof(poly_memkv_iter_t));
    if (!new_iter) return INFRA_ERROR_NO_MEMORY;

    // 初始化迭代器
    new_iter->store = store;
    new_iter->engine_iter = NULL;

    // 获取插件接口
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    if (!interface) {
        infra_free(new_iter);
        return INFRA_ERROR_INVALID_STATE;
    }

    // 创建引擎迭代器
    if (store->config.engine_type == POLY_MEMKV_ENGINE_SQLITE) {
        const poly_sqlite_interface_t* sqlite_interface = (const poly_sqlite_interface_t*)interface;
        infra_error_t err = sqlite_interface->iter_create(store->engine_handle, &new_iter->engine_iter);
        if (err != INFRA_OK) {
            infra_free(new_iter);
            return err;
        }
    } else {
        const poly_duckdb_interface_t* duckdb_interface = (const poly_duckdb_interface_t*)interface;
        infra_error_t err = duckdb_interface->iter_create(store->engine_handle, &new_iter->engine_iter);
        if (err != INFRA_OK) {
            infra_free(new_iter);
            return err;
        }
    }

    *iter = new_iter;
    return INFRA_OK;
}

// 迭代下一个键值对
infra_error_t poly_memkv_iter_next(poly_memkv_iter_t* iter, char** key, size_t* key_len,
                                  void** value, size_t* value_len) {
    if (!iter || !key || !key_len || !value || !value_len) return INFRA_ERROR_INVALID_PARAM;

    // 获取插件接口
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(iter->store->engine_plugin);
    if (!interface) return INFRA_ERROR_INVALID_STATE;

    // 调用对应引擎的迭代器
    if (iter->store->config.engine_type == POLY_MEMKV_ENGINE_SQLITE) {
        const poly_sqlite_interface_t* sqlite_interface = (const poly_sqlite_interface_t*)interface;
        return sqlite_interface->iter_next(iter->engine_iter, key, value, value_len);
    } else {
        const poly_duckdb_interface_t* duckdb_interface = (const poly_duckdb_interface_t*)interface;
        return duckdb_interface->iter_next(iter->engine_iter, key, value, value_len);
    }
}

// 销毁迭代器
void poly_memkv_iter_destroy(poly_memkv_iter_t* iter) {
    if (!iter) return;

    // 获取插件接口
    const poly_plugin_interface_t* interface = poly_plugin_get_interface(iter->store->engine_plugin);
    if (!interface) {
        infra_free(iter);
        return;
    }

    // 销毁引擎迭代器
    if (iter->store->config.engine_type == POLY_MEMKV_ENGINE_SQLITE) {
        const poly_sqlite_interface_t* sqlite_interface = (const poly_sqlite_interface_t*)interface;
        sqlite_interface->iter_destroy(iter->engine_iter);
    } else {
        const poly_duckdb_interface_t* duckdb_interface = (const poly_duckdb_interface_t*)interface;
        duckdb_interface->iter_destroy(iter->engine_iter);
    }

    infra_free(iter);
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

    // 更新配置
    infra_error_t err = poly_memkv_configure(store, config);
    if (err != INFRA_OK) return err;

    // 重新打开存储
    return poly_memkv_open(store);
}

// ... 其他函数的实现 ...