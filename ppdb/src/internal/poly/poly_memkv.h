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
    POLY_MEMKV_ENGINE_SQLITE,  // SQLite引擎(默认)
    POLY_MEMKV_ENGINE_DUCKDB   // DuckDB引擎
} poly_memkv_engine_type_t;

// 配置参数
typedef struct {
    size_t max_key_size;          // 最大键长度
    size_t max_value_size;        // 最大值长度
    poly_memkv_engine_type_t engine_type;  // 存储引擎类型
    char* path;                   // 存储路径
} poly_memkv_config_t;

// 统计信息
typedef struct {
    poly_atomic_t cmd_get;         // GET命令次数
    poly_atomic_t cmd_set;         // SET命令次数
    poly_atomic_t cmd_del;         // DELETE命令次数
    poly_atomic_t curr_items;      // 当前项目数
    poly_atomic_t hits;            // 缓存命中次数
    poly_atomic_t misses;          // 缓存未命中次数
} poly_memkv_stats_t;

// 引擎句柄类型
typedef struct poly_sqlite_db poly_sqlite_db_t;
typedef struct poly_duckdb_db poly_duckdb_db_t;

// 引擎迭代器类型
typedef struct poly_sqlite_iter poly_sqlite_iter_t;
typedef struct poly_duckdb_iter poly_duckdb_iter_t;

// 存储实例
typedef struct poly_memkv {
    poly_memkv_config_t config;    // 配置参数
    poly_memkv_stats_t stats;      // 统计信息
    poly_plugin_mgr_t* plugin_mgr; // 插件管理器
    poly_plugin_t* engine_plugin;  // 存储引擎插件
    void* engine_handle;           // 存储引擎句柄
} poly_memkv_t;

// 迭代器结构体
typedef struct poly_memkv_iter {
    poly_memkv_t* store;
    void* engine_iter;
} poly_memkv_iter_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// 创建 MemKV 实例
infra_error_t poly_memkv_create(poly_memkv_t** store);

// 配置 MemKV 实例
infra_error_t poly_memkv_configure(poly_memkv_t* store, const poly_memkv_config_t* config);

// 打开存储
infra_error_t poly_memkv_open(poly_memkv_t* store);

// 关闭存储
void poly_memkv_close(poly_memkv_t* store);

// 销毁 MemKV 实例
void poly_memkv_destroy(poly_memkv_t* store);

// 设置键值对
infra_error_t poly_memkv_set(poly_memkv_t* store, const char* key, size_t key_len,
    const void* value, size_t value_len);

// 获取键值对
infra_error_t poly_memkv_get(poly_memkv_t* store, const char* key, size_t key_len,
    void** value, size_t* value_len);

// 删除键值对
infra_error_t poly_memkv_del(poly_memkv_t* store, const char* key, size_t key_len);

// 创建迭代器
infra_error_t poly_memkv_iter_create(poly_memkv_t* store, poly_memkv_iter_t** iter);

// 迭代下一个键值对
infra_error_t poly_memkv_iter_next(poly_memkv_iter_t* iter, char** key, size_t* key_len,
    void** value, size_t* value_len);

// 销毁迭代器
void poly_memkv_iter_destroy(poly_memkv_iter_t* iter);

// 获取存储引擎类型
poly_memkv_engine_type_t poly_memkv_get_engine_type(const poly_memkv_t* store);

/**
 * @brief 获取 MemKV 统计信息
 * @param store MemKV 存储实例
 * @return 统计信息指针，如果失败返回 NULL
 */
const poly_memkv_stats_t* poly_memkv_get_stats(poly_memkv_t* store);

/**
 * @brief 切换存储引擎
 * @param store MemKV 存储实例
 * @param engine_type 目标引擎类型
 * @param config 引擎配置
 * @return 错误码
 */
infra_error_t poly_memkv_switch_engine(poly_memkv_t* store, 
                                     poly_memkv_engine_type_t engine_type,
                                     const poly_memkv_config_t* config);

#endif // POLY_MEMKV_H 