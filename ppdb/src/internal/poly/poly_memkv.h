#ifndef POLY_MEMKV_H
#define POLY_MEMKV_H

#include "internal/infra/infra_core.h"
#include "internal/poly/poly_plugin.h"
#include "internal/poly/poly_atomic.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 存储引擎类型
typedef enum {
    POLY_MEMKV_ENGINE_SQLITE = 0,  // SQLite引擎(默认)
    POLY_MEMKV_ENGINE_DUCKDB = 1   // DuckDB引擎
} poly_memkv_engine_type_t;

// 配置参数
typedef struct {
    size_t max_key_size;           // 最大键长度
    size_t max_value_size;         // 最大值长度
    poly_memkv_engine_type_t engine_type;  // 存储引擎类型
    const char* plugin_path;       // 插件路径(DuckDB)
} poly_memkv_config_t;

// 统计信息
typedef struct {
    poly_atomic_t hits;            // 缓存命中次数
    poly_atomic_t misses;          // 缓存未命中次数
    poly_atomic_t curr_items;      // 当前项目数
    poly_atomic_t total_items;     // 总项目数
    poly_atomic_t bytes;           // 总字节数
    poly_atomic_t cmd_get;         // GET命令次数
    poly_atomic_t cmd_set;         // SET命令次数
} poly_memkv_stats_t;

// 存储实例
typedef struct poly_memkv {
    poly_memkv_config_t config;    // 配置参数
    poly_memkv_stats_t stats;      // 统计信息
    poly_plugin_mgr_t* plugins;    // 插件管理器
    poly_plugin_t* engine_plugin;  // 存储引擎插件
    void* engine_handle;           // 存储引擎句柄
    poly_memkv_engine_type_t type; // 当前引擎类型
} poly_memkv_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// 创建存储实例
infra_error_t poly_memkv_create(const poly_memkv_config_t* config, poly_memkv_t** store);

// 销毁存储实例
void poly_memkv_destroy(poly_memkv_t* store);

// 设置键值对
infra_error_t poly_memkv_set(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size);

// 获取键值对
infra_error_t poly_memkv_get(poly_memkv_t* store, const char* key,
    void** value, size_t* value_size);

// 删除键值对
infra_error_t poly_memkv_del(poly_memkv_t* store, const char* key);

// 增加计数器
infra_error_t poly_memkv_incr(poly_memkv_t* store, const char* key,
    uint64_t delta, uint64_t* new_value);

// 减少计数器
infra_error_t poly_memkv_decr(poly_memkv_t* store, const char* key,
    uint64_t delta, uint64_t* new_value);

// 获取统计信息
const poly_memkv_stats_t* poly_memkv_get_stats(poly_memkv_t* store);

// 切换存储引擎
infra_error_t poly_memkv_switch_engine(poly_memkv_t* store, 
    poly_memkv_engine_type_t type, const char* plugin_path);

#endif // POLY_MEMKV_H 