#include "internal/poly/poly_memkv.h"
#include "internal/infra/infra_core.h"
#include "internal/poly/poly_hashtable.h"
#include "internal/poly/poly_atomic.h"
#include "internal/poly/poly_plugin.h"
#include "internal/poly/poly_sqlite.h"
#include "internal/poly/poly_duckdb.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 内存KV存储实现
struct poly_memkv {
    poly_hashtable_t* cache;        // 内存缓存
    poly_memkv_config_t config;     // 配置信息
    poly_memkv_stats_t stats;       // 统计信息
    infra_mutex_t mutex;            // 全局互斥锁
    poly_atomic_t cas_counter;      // CAS计数器
    poly_plugin_mgr_t* plugins;     // 插件管理器
    poly_plugin_t* engine_plugin;   // 当前存储引擎插件
    void* engine_handle;            // 存储引擎句柄
    poly_memkv_engine_type_t type;  // 当前引擎类型
};

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 创建存储项
static poly_memkv_item_t* create_item(const char* key, const void* value, 
    size_t value_size, uint32_t flags, uint32_t exptime) {
    
    if (!key || !value || value_size == 0) {
        return NULL;
    }

    poly_memkv_item_t* item = malloc(sizeof(poly_memkv_item_t));
    if (!item) {
        return NULL;
    }

    item->key = strdup(key);
    if (!item->key) {
        free(item);
        return NULL;
    }

    item->value = malloc(value_size);
    if (!item->value) {
        free(item->key);
        free(item);
        return NULL;
    }

    memcpy(item->value, value, value_size);
    item->value_size = value_size;
    item->flags = flags;
    item->exptime = exptime ? time(NULL) + exptime : 0;
    item->next = NULL;

    return item;
}

// 检查项目是否过期
bool poly_memkv_is_expired(const poly_memkv_item_t* item) {
    if (!item || item->exptime == 0) {
        return false;
    }
    return time(NULL) > item->exptime;
}

// 释放项目
void poly_memkv_free_item(poly_memkv_item_t* item) {
    if (!item) {
        return;
    }
    if (item->key) {
        free(item->key);
    }
    if (item->value) {
        free(item->value);
    }
    free(item);
}

//-----------------------------------------------------------------------------
// Static Functions
//-----------------------------------------------------------------------------

// 初始化默认的SQLite引擎
static infra_error_t init_sqlite_engine(poly_memkv_t* store) {
    // 注册内置的SQLite插件
    poly_builtin_plugin_t sqlite_plugin = {
        .name = "sqlite",
        .interface = &g_sqlite_interface,
        .type = POLY_PLUGIN_SQLITE
    };
    
    infra_error_t err = poly_plugin_register_builtin(store->plugins, &sqlite_plugin);
    if (err != INFRA_OK) {
        return err;
    }
    
    store->engine_plugin = (poly_plugin_t*)&sqlite_plugin;
    store->type = POLY_MEMKV_ENGINE_SQLITE;
    
    // 初始化SQLite引擎
    poly_sqlite_interface_t* sqlite = poly_plugin_get_interface(store->engine_plugin);
    return sqlite->init(&store->engine_handle);
}

// 初始化DuckDB引擎
static infra_error_t init_duckdb_engine(poly_memkv_t* store, const char* plugin_path) {
    // 加载DuckDB插件
    infra_error_t err = poly_plugin_mgr_load(store->plugins, 
        POLY_PLUGIN_DUCKDB, plugin_path, &store->engine_plugin);
    if (err != INFRA_OK) {
        return err;
    }
    
    store->type = POLY_MEMKV_ENGINE_DUCKDB;
    
    // 初始化DuckDB引擎
    poly_duckdb_interface_t* duckdb = poly_plugin_get_interface(store->engine_plugin);
    return duckdb->init(&store->engine_handle);
}

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

// 创建存储实例
infra_error_t poly_memkv_create(const poly_memkv_config_t* config, poly_memkv_t** store) {
    if (!config || !store) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    *store = calloc(1, sizeof(poly_memkv_t));
    if (!*store) {
        return INFRA_ERROR_NO_MEMORY;
    }
    
    // 创建插件管理器
    infra_error_t err = poly_plugin_mgr_create(&(*store)->plugins);
    if (err != INFRA_OK) {
        free(*store);
        return err;
    }
    
    // 创建内存缓存
    err = poly_hashtable_create(config->initial_size, &(*store)->cache);
    if (err != INFRA_OK) {
        poly_plugin_mgr_destroy((*store)->plugins);
        free(*store);
        return err;
    }
    
    // 初始化存储引擎
    if (config->engine_type == POLY_MEMKV_ENGINE_DUCKDB) {
        err = init_duckdb_engine(*store, config->plugin_path);
    } else {
        err = init_sqlite_engine(*store);
    }
    
    if (err != INFRA_OK) {
        poly_hashtable_destroy((*store)->cache);
        poly_plugin_mgr_destroy((*store)->plugins);
        free(*store);
        return err;
    }
    
    return INFRA_OK;
}

void poly_memkv_destroy(poly_memkv_t* store) {
    if (!store) {
        return;
    }
    
    // 清理存储引擎
    if (store->engine_plugin) {
        if (store->type == POLY_MEMKV_ENGINE_SQLITE) {
            poly_sqlite_interface_t* sqlite = poly_plugin_get_interface(store->engine_plugin);
            sqlite->cleanup(store->engine_handle);
        } else {
            poly_duckdb_interface_t* duckdb = poly_plugin_get_interface(store->engine_plugin);
            duckdb->cleanup(store->engine_handle);
            poly_plugin_mgr_unload(store->plugins, store->engine_plugin);
        }
    }
    
    // 清理其他资源
    poly_hashtable_destroy(store->cache);
    poly_plugin_mgr_destroy(store->plugins);
    free(store);
}

// 设置CAS值
static uint64_t get_next_cas(poly_memkv_t* store) {
    poly_atomic_inc(&store->cas_counter);
    return (uint64_t)poly_atomic_get(&store->cas_counter);
}

infra_error_t poly_memkv_set(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size, uint32_t flags, uint32_t exptime) {
    
    if (!store || !key || !value || value_size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (strlen(key) > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (value_size > store->config.max_value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建新项目
    poly_memkv_item_t* item = create_item(key, value, value_size, flags, exptime);
    if (!item) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 设置CAS值
    item->cas = get_next_cas(store);

    // 加锁
    infra_mutex_lock(&store->mutex);

    // 检查是否存在旧项目
    void* old_value = NULL;
    infra_error_t get_err = poly_hashtable_get(store->cache, key, &old_value);
    if (get_err == INFRA_OK && old_value) {
        poly_memkv_item_t* old_item = (poly_memkv_item_t*)old_value;
        poly_atomic_sub(&store->stats.bytes, old_item->value_size);
    } else {
        poly_atomic_inc(&store->stats.curr_items);
        poly_atomic_inc(&store->stats.total_items);
    }

    // 更新统计信息
    poly_atomic_inc(&store->stats.cmd_set);
    poly_atomic_add(&store->stats.bytes, value_size);

    // 存储项目
    infra_error_t err = poly_hashtable_put(store->cache, item->key, item);

    // 如果存储成功且存在旧项目，释放旧项目
    if (err == INFRA_OK && old_value) {
        poly_memkv_free_item((poly_memkv_item_t*)old_value);
    }

    // 解锁
    infra_mutex_unlock(&store->mutex);

    if (err != INFRA_OK) {
        poly_memkv_free_item(item);
        return err;
    }

    return INFRA_OK;
}

infra_error_t poly_memkv_get(poly_memkv_t* store, const char* key,
    poly_memkv_item_t** item) {
    
    if (!store || !key || !item) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 加锁
    infra_mutex_lock(&store->mutex);

    // 获取项目
    void* value = NULL;
    infra_error_t err = poly_hashtable_get(store->cache, key, &value);
    
    if (err == INFRA_OK && value) {
        poly_memkv_item_t* found = (poly_memkv_item_t*)value;
        
        // 检查是否过期
        if (poly_memkv_is_expired(found)) {
            // 移除过期项目
            poly_hashtable_remove(store->cache, key);
            poly_atomic_dec(&store->stats.curr_items);
            poly_atomic_sub(&store->stats.bytes, found->value_size);
            poly_memkv_free_item(found);
            
            poly_atomic_inc(&store->stats.misses);
            err = INFRA_ERROR_NOT_FOUND;
        } else {
            *item = found;
            poly_atomic_inc(&store->stats.hits);
        }
    } else {
        poly_atomic_inc(&store->stats.misses);
    }

    // 解锁
    infra_mutex_unlock(&store->mutex);

    return err;
}

const poly_memkv_stats_t* poly_memkv_get_stats(poly_memkv_t* store) {
    return store ? &store->stats : NULL;
}

infra_error_t poly_memkv_add(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size, uint32_t flags, uint32_t exptime) {
    
    if (!store || !key || !value || value_size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (strlen(key) > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (value_size > store->config.max_value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 加锁
    infra_mutex_lock(&store->mutex);

    // 检查key是否存在
    void* existing = NULL;
    infra_error_t err = poly_hashtable_get(store->cache, key, &existing);
    
    if (err == INFRA_OK && existing) {
        poly_memkv_item_t* item = (poly_memkv_item_t*)existing;
        
        // 检查是否过期
        if (poly_memkv_is_expired(item)) {
            // 移除过期项目
            poly_hashtable_remove(store->cache, key);
            poly_atomic_dec(&store->stats.curr_items);
            poly_atomic_sub(&store->stats.bytes, item->value_size);
            poly_memkv_free_item(item);
        } else {
            infra_mutex_unlock(&store->mutex);
            return INFRA_ERROR_ALREADY_EXISTS;
        }
    }

    // 创建新项目
    poly_memkv_item_t* new_item = create_item(key, value, value_size, flags, exptime);
    if (!new_item) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 设置CAS值
    new_item->cas = get_next_cas(store);

    // 更新统计信息
    poly_atomic_inc(&store->stats.curr_items);
    poly_atomic_inc(&store->stats.total_items);
    poly_atomic_add(&store->stats.bytes, value_size);

    // 存储项目
    err = poly_hashtable_put(store->cache, new_item->key, new_item);
    if (err != INFRA_OK) {
        poly_memkv_free_item(new_item);
        poly_atomic_dec(&store->stats.curr_items);
        poly_atomic_dec(&store->stats.total_items);
        poly_atomic_sub(&store->stats.bytes, value_size);
    }

    // 解锁
    infra_mutex_unlock(&store->mutex);

    return err;
}

infra_error_t poly_memkv_replace(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size, uint32_t flags, uint32_t exptime) {
    
    if (!store || !key || !value || value_size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (strlen(key) > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (value_size > store->config.max_value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 加锁
    infra_mutex_lock(&store->mutex);

    // 检查key是否存在
    void* existing = NULL;
    infra_error_t err = poly_hashtable_get(store->cache, key, &existing);
    
    if (err != INFRA_OK || !existing) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    poly_memkv_item_t* item = (poly_memkv_item_t*)existing;
    
    // 检查是否过期
    if (poly_memkv_is_expired(item)) {
        // 移除过期项目
        poly_hashtable_remove(store->cache, key);
        poly_atomic_dec(&store->stats.curr_items);
        poly_atomic_sub(&store->stats.bytes, item->value_size);
        poly_memkv_free_item(item);
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 创建新项目
    poly_memkv_item_t* new_item = create_item(key, value, value_size, flags, exptime);
    if (!new_item) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 设置CAS值
    new_item->cas = get_next_cas(store);

    // 更新统计信息
    poly_atomic_sub(&store->stats.bytes, item->value_size);
    poly_atomic_add(&store->stats.bytes, value_size);

    // 存储新项目
    err = poly_hashtable_put(store->cache, new_item->key, new_item);
    if (err != INFRA_OK) {
        poly_memkv_free_item(new_item);
        poly_atomic_add(&store->stats.bytes, item->value_size);
        poly_atomic_sub(&store->stats.bytes, value_size);
    } else {
        poly_memkv_free_item(item);
    }

    // 解锁
    infra_mutex_unlock(&store->mutex);

    return err;
}

infra_error_t poly_memkv_delete(poly_memkv_t* store, const char* key) {
    if (!store || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 加锁
    infra_mutex_lock(&store->mutex);

    // 检查key是否存在
    void* existing = NULL;
    infra_error_t err = poly_hashtable_get(store->cache, key, &existing);
    
    if (err != INFRA_OK || !existing) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    poly_memkv_item_t* item = (poly_memkv_item_t*)existing;
    
    // 检查是否过期
    if (poly_memkv_is_expired(item)) {
        // 移除过期项目
        err = poly_hashtable_remove(store->cache, key);
        poly_atomic_dec(&store->stats.curr_items);
        poly_atomic_sub(&store->stats.bytes, item->value_size);
        poly_memkv_free_item(item);
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 移除项目
    poly_hashtable_remove(store->cache, key);
    poly_atomic_dec(&store->stats.curr_items);
    poly_atomic_sub(&store->stats.bytes, item->value_size);
    poly_memkv_free_item(item);

    // 解锁
    infra_mutex_unlock(&store->mutex);

    return err;
}

infra_error_t poly_memkv_append(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size) {
    
    if (!store || !key || !value || value_size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 加锁
    infra_mutex_lock(&store->mutex);

    // 检查key是否存在
    void* existing = NULL;
    infra_error_t err = poly_hashtable_get(store->cache, key, &existing);
    
    if (err != INFRA_OK || !existing) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    poly_memkv_item_t* item = (poly_memkv_item_t*)existing;
    
    // 检查是否过期
    if (poly_memkv_is_expired(item)) {
        // 移除过期项目
        poly_hashtable_remove(store->cache, key);
        poly_atomic_dec(&store->stats.curr_items);
        poly_atomic_sub(&store->stats.bytes, item->value_size);
        poly_memkv_free_item(item);
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 检查新大小是否超过限制
    size_t new_size = item->value_size + value_size;
    if (new_size > store->config.max_value_size) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建新项目
    void* new_value = malloc(new_size);
    if (!new_value) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 复制原有数据
    memcpy(new_value, item->value, item->value_size);
    // 追加新数据
    memcpy((char*)new_value + item->value_size, value, value_size);

    // 创建新项目
    poly_memkv_item_t* new_item = create_item(key, new_value, new_size, item->flags, item->exptime);
    free(new_value);

    if (!new_item) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 设置CAS值
    new_item->cas = get_next_cas(store);

    // 更新统计信息
    poly_atomic_sub(&store->stats.bytes, item->value_size);
    poly_atomic_add(&store->stats.bytes, new_size);

    // 存储新项目
    err = poly_hashtable_put(store->cache, new_item->key, new_item);
    if (err != INFRA_OK) {
        poly_memkv_free_item(new_item);
        poly_atomic_add(&store->stats.bytes, item->value_size);
        poly_atomic_sub(&store->stats.bytes, new_size);
    } else {
        poly_memkv_free_item(item);
    }

    // 解锁
    infra_mutex_unlock(&store->mutex);

    return err;
}

infra_error_t poly_memkv_prepend(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size) {
    
    if (!store || !key || !value || value_size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 加锁
    infra_mutex_lock(&store->mutex);

    // 检查key是否存在
    void* existing = NULL;
    infra_error_t err = poly_hashtable_get(store->cache, key, &existing);
    
    if (err != INFRA_OK || !existing) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    poly_memkv_item_t* item = (poly_memkv_item_t*)existing;
    
    // 检查是否过期
    if (poly_memkv_is_expired(item)) {
        // 移除过期项目
        poly_hashtable_remove(store->cache, key);
        poly_atomic_dec(&store->stats.curr_items);
        poly_atomic_sub(&store->stats.bytes, item->value_size);
        poly_memkv_free_item(item);
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 检查新大小是否超过限制
    size_t new_size = item->value_size + value_size;
    if (new_size > store->config.max_value_size) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建新项目
    void* new_value = malloc(new_size);
    if (!new_value) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 复制新数据
    memcpy(new_value, value, value_size);
    // 追加原有数据
    memcpy((char*)new_value + value_size, item->value, item->value_size);

    // 创建新项目
    poly_memkv_item_t* new_item = create_item(key, new_value, new_size, item->flags, item->exptime);
    free(new_value);

    if (!new_item) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 设置CAS值
    new_item->cas = get_next_cas(store);

    // 更新统计信息
    poly_atomic_sub(&store->stats.bytes, item->value_size);
    poly_atomic_add(&store->stats.bytes, new_size);

    // 存储新项目
    err = poly_hashtable_put(store->cache, new_item->key, new_item);
    if (err != INFRA_OK) {
        poly_memkv_free_item(new_item);
        poly_atomic_add(&store->stats.bytes, item->value_size);
        poly_atomic_sub(&store->stats.bytes, new_size);
    } else {
        poly_memkv_free_item(item);
    }

    // 解锁
    infra_mutex_unlock(&store->mutex);

    return err;
}

infra_error_t poly_memkv_cas(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size, uint32_t flags, uint32_t exptime,
    uint64_t cas) {
    
    if (!store || !key || !value || value_size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (strlen(key) > store->config.max_key_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (value_size > store->config.max_value_size) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 加锁
    infra_mutex_lock(&store->mutex);

    // 检查key是否存在
    void* existing = NULL;
    infra_error_t err = poly_hashtable_get(store->cache, key, &existing);
    
    if (err != INFRA_OK || !existing) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    poly_memkv_item_t* item = (poly_memkv_item_t*)existing;
    
    // 检查是否过期
    if (poly_memkv_is_expired(item)) {
        // 移除过期项目
        poly_hashtable_remove(store->cache, key);
        poly_atomic_dec(&store->stats.curr_items);
        poly_atomic_sub(&store->stats.bytes, item->value_size);
        poly_memkv_free_item(item);
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 检查CAS值
    if (item->cas != cas) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_CAS_MISMATCH;
    }

    // 创建新项目
    poly_memkv_item_t* new_item = create_item(key, value, value_size, flags, exptime);
    if (!new_item) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 设置CAS值
    new_item->cas = get_next_cas(store);

    // 更新统计信息
    poly_atomic_sub(&store->stats.bytes, item->value_size);
    poly_atomic_add(&store->stats.bytes, value_size);

    // 存储新项目
    err = poly_hashtable_put(store->cache, new_item->key, new_item);
    if (err != INFRA_OK) {
        poly_memkv_free_item(new_item);
        poly_atomic_add(&store->stats.bytes, item->value_size);
        poly_atomic_sub(&store->stats.bytes, value_size);
    } else {
        poly_memkv_free_item(item);
    }

    // 解锁
    infra_mutex_unlock(&store->mutex);

    return err;
}

infra_error_t poly_memkv_flush(poly_memkv_t* store) {
    if (!store) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 加锁
    infra_mutex_lock(&store->mutex);

    // 清空哈希表
    poly_hashtable_clear(store->cache);

    // 重置统计信息
    memset(&store->stats, 0, sizeof(poly_memkv_stats_t));

    // 解锁
    infra_mutex_unlock(&store->mutex);

    return INFRA_OK;
}

// 增加值
infra_error_t poly_memkv_incr(poly_memkv_t* store, const char* key,
    uint64_t delta, uint64_t* new_value) {
    if (!store || !key || !new_value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 加锁
    infra_mutex_lock(&store->mutex);

    // 获取项目
    void* value = NULL;
    infra_error_t err = poly_hashtable_get(store->cache, key, &value);
    if (err != INFRA_OK) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    poly_memkv_item_t* item = (poly_memkv_item_t*)value;
    
    // 检查是否过期
    if (poly_memkv_is_expired(item)) {
        // 移除过期项目
        poly_hashtable_remove(store->cache, key);
        poly_atomic_dec(&store->stats.curr_items);
        poly_atomic_sub(&store->stats.bytes, item->value_size);
        poly_memkv_free_item(item);
        
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 检查值是否为数字
    char* endptr;
    uint64_t curr_value = strtoull(item->value, &endptr, 10);
    if (*endptr != '\0') {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_INVALID_TYPE;
    }

    // 计算新值
    uint64_t value_new = curr_value + delta;
    
    // 转换为字符串
    char value_str[32];
    size_t value_len = snprintf(value_str, sizeof(value_str), "%lu", value_new);
    
    // 分配新内存
    void* new_data = malloc(value_len + 1);
    if (!new_data) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    // 更新数据
    memcpy(new_data, value_str, value_len + 1);
    free(item->value);
    item->value = new_data;
    item->value_size = value_len;
    item->cas = get_next_cas(store);
    
    *new_value = value_new;
    
    infra_mutex_unlock(&store->mutex);
    return INFRA_OK;
}

// 减少值
infra_error_t poly_memkv_decr(poly_memkv_t* store, const char* key,
    uint64_t delta, uint64_t* new_value) {
    if (!store || !key || !new_value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 加锁
    infra_mutex_lock(&store->mutex);

    // 获取项目
    void* value = NULL;
    infra_error_t err = poly_hashtable_get(store->cache, key, &value);
    if (err != INFRA_OK) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    poly_memkv_item_t* item = (poly_memkv_item_t*)value;
    
    // 检查是否过期
    if (poly_memkv_is_expired(item)) {
        // 移除过期项目
        poly_hashtable_remove(store->cache, key);
        poly_atomic_dec(&store->stats.curr_items);
        poly_atomic_sub(&store->stats.bytes, item->value_size);
        poly_memkv_free_item(item);
        
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 检查值是否为数字
    char* endptr;
    uint64_t curr_value = strtoull(item->value, &endptr, 10);
    if (*endptr != '\0') {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_INVALID_TYPE;
    }

    // 计算新值（不能小于0）
    uint64_t value_new = (curr_value > delta) ? (curr_value - delta) : 0;
    
    // 转换为字符串
    char value_str[32];
    size_t value_len = snprintf(value_str, sizeof(value_str), "%lu", value_new);
    
    // 分配新内存
    void* new_data = malloc(value_len + 1);
    if (!new_data) {
        infra_mutex_unlock(&store->mutex);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    // 更新数据
    memcpy(new_data, value_str, value_len + 1);
    free(item->value);
    item->value = new_data;
    item->value_size = value_len;
    item->cas = get_next_cas(store);
    
    *new_value = value_new;
    
    infra_mutex_unlock(&store->mutex);
    return INFRA_OK;
}

infra_error_t poly_memkv_switch_engine(poly_memkv_t* store, 
    poly_memkv_engine_type_t type, const char* plugin_path) {
    if (!store) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // 如果类型相同且不是DuckDB,无需切换
    if (store->type == type && type != POLY_MEMKV_ENGINE_DUCKDB) {
        return INFRA_OK;
    }
    
    // 保存旧引擎数据
    void* old_handle = store->engine_handle;
    poly_plugin_t* old_plugin = store->engine_plugin;
    poly_memkv_engine_type_t old_type = store->type;
    
    // 初始化新引擎
    infra_error_t err;
    if (type == POLY_MEMKV_ENGINE_DUCKDB) {
        err = init_duckdb_engine(store, plugin_path);
    } else {
        err = init_sqlite_engine(store);
    }
    
    if (err != INFRA_OK) {
        // 恢复旧引擎
        store->engine_handle = old_handle;
        store->engine_plugin = old_plugin;
        store->type = old_type;
        return err;
    }
    
    // 清理旧引擎
    if (old_type == POLY_MEMKV_ENGINE_SQLITE) {
        poly_sqlite_interface_t* sqlite = poly_plugin_get_interface(old_plugin);
        sqlite->cleanup(old_handle);
    } else {
        poly_duckdb_interface_t* duckdb = poly_plugin_get_interface(old_plugin);
        duckdb->cleanup(old_handle);
        poly_plugin_mgr_unload(store->plugins, old_plugin);
    }
    
    return INFRA_OK;
}

poly_memkv_engine_type_t poly_memkv_get_engine_type(const poly_memkv_t* store) {
    return store ? store->type : POLY_MEMKV_ENGINE_SQLITE;
}

// ... 其他函数的实现 ...