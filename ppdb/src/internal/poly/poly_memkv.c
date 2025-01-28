#include "internal/poly/poly_memkv.h"
#include "internal/infra/infra_core.h"
#include "internal/poly/poly_sqlitekv.h"
#include "internal/poly/poly_duckdbkv.h"

//-----------------------------------------------------------------------------
// Static functions
//-----------------------------------------------------------------------------

// 初始化 SQLite KV 引擎
static infra_error_t init_sqlite_engine(poly_memkv_t* store) {
    if (!store) return INFRA_ERROR_INVALID_PARAM;

    // 注册 SQLite KV 插件
    poly_builtin_plugin_t plugin = {
        .name = "sqlite",
        .type = POLY_PLUGIN_SQLITE,
        .interface = (const poly_plugin_interface_t*)&g_sqlitekv_interface
    };

    infra_error_t err = poly_plugin_register_builtin(store->plugin_mgr, &plugin);
    if (err != INFRA_OK) return err;

    // 获取插件实例
    return poly_plugin_mgr_get(store->plugin_mgr, POLY_PLUGIN_SQLITE, "sqlite", &store->engine_plugin);
}

// 初始化 DuckDB KV 引擎
static infra_error_t init_duckdb_engine(poly_memkv_t* store) {
    if (!store) return INFRA_ERROR_INVALID_PARAM;

    // 注册 DuckDB KV 插件
    poly_builtin_plugin_t plugin = {
        .name = "duckdb",
        .type = POLY_PLUGIN_DUCKDB,
        .interface = (const poly_plugin_interface_t*)&g_duckdbkv_interface
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
    if (!store || !key || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    if (!interface) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 如果存在旧值，先获取它
    void* old_value = NULL;
    size_t old_value_len = 0;
    infra_error_t get_err = interface->get(store->engine_handle, key, key_len, &old_value, &old_value_len);
    if (get_err == INFRA_OK) {
        infra_free(old_value);
    }

    // 设置新值
    infra_error_t err = interface->set(store->engine_handle, key, key_len, value, value_len);
    if (err != INFRA_OK) {
        return err;
    }

    // 更新统计信息
    if (get_err == INFRA_ERROR_NOT_FOUND) {
        poly_atomic_inc(&store->stats.curr_items);
    }
    poly_atomic_inc(&store->stats.cmd_set);
    if (get_err == INFRA_OK) {
        poly_atomic_dec(&store->stats.curr_items);
    }

    return INFRA_OK;
}

// 获取键值对
infra_error_t poly_memkv_get(poly_memkv_t* store, const char* key, size_t key_len,
                            void** value, size_t* value_len) {
    if (!store || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    if (!interface) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    return interface->get(store->engine_handle, key, key_len, value, value_len);
}

// 删除键值对
infra_error_t poly_memkv_del(poly_memkv_t* store, const char* key, size_t key_len) {
    if (!store || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    const poly_plugin_interface_t* interface = poly_plugin_get_interface(store->engine_plugin);
    if (!interface) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 如果存在值，先获取它以更新统计信息
    void* value = NULL;
    size_t value_len = 0;
    infra_error_t get_err = interface->get(store->engine_handle, key, key_len, &value, &value_len);
    if (get_err == INFRA_OK) {
        poly_atomic_dec(&store->stats.curr_items);
        poly_atomic_inc(&store->stats.cmd_del);
        infra_free(value);
    }

    return interface->del(store->engine_handle, key, key_len);
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
        const poly_sqlitekv_interface_t* sqlite_interface = (const poly_sqlitekv_interface_t*)interface;
        infra_error_t err = sqlite_interface->iter_create(store->engine_handle, &new_iter->engine_iter);
        if (err != INFRA_OK) {
            infra_free(new_iter);
            return err;
        }
    } else {
        const poly_duckdbkv_interface_t* duckdb_interface = (const poly_duckdbkv_interface_t*)interface;
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
        const poly_sqlitekv_interface_t* sqlite_interface = (const poly_sqlitekv_interface_t*)interface;
        return sqlite_interface->iter_next(iter->engine_iter, key, value, value_len);
    } else {
        const poly_duckdbkv_interface_t* duckdb_interface = (const poly_duckdbkv_interface_t*)interface;
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
        const poly_sqlitekv_interface_t* sqlite_interface = (const poly_sqlitekv_interface_t*)interface;
        sqlite_interface->iter_destroy(iter->engine_iter);
    } else {
        const poly_duckdbkv_interface_t* duckdb_interface = (const poly_duckdbkv_interface_t*)interface;
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

// ... 其他函数的实现 ...