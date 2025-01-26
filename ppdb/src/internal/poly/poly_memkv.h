#ifndef POLY_MEMKV_H
#define POLY_MEMKV_H

#include "internal/infra/infra_core.h"
#include "internal/poly/poly_hashtable.h"
#include "internal/poly/poly_atomic.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 内存KV存储项
typedef struct poly_memkv_item {
    char* key;                  // 键
    void* value;                // 值
    size_t value_size;          // 值大小
    uint32_t flags;             // 标志位
    uint32_t exptime;          // 过期时间
    uint64_t cas;              // CAS值
    struct poly_memkv_item* next;   // 链表下一项
} poly_memkv_item_t;

// 内存KV存储统计信息
typedef struct poly_memkv_stats {
    poly_atomic_t cmd_get;      // Get命令次数
    poly_atomic_t cmd_set;      // Set命令次数
    poly_atomic_t cmd_delete;   // Delete命令次数
    poly_atomic_t hits;         // 缓存命中次数
    poly_atomic_t misses;       // 缓存未命中次数
    poly_atomic_t curr_items;   // 当前项目数
    poly_atomic_t total_items;  // 总项目数
    poly_atomic_t bytes;        // 当前使用字节数
    poly_atomic_t curr_connections;  // 当前连接数
    poly_atomic_t total_connections; // 总连接数
} poly_memkv_stats_t;

// 内存KV存储配置
typedef struct poly_memkv_config {
    size_t initial_size;        // 初始哈希表大小
    size_t max_key_size;        // 最大键长度
    size_t max_value_size;      // 最大值长度
} poly_memkv_config_t;

// 内存KV存储句柄
typedef struct poly_memkv poly_memkv_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// 创建内存KV存储
infra_error_t poly_memkv_create(const poly_memkv_config_t* config, poly_memkv_t** store);

// 销毁内存KV存储
void poly_memkv_destroy(poly_memkv_t* store);

// 设置键值对
infra_error_t poly_memkv_set(poly_memkv_t* store, const char* key, 
    const void* value, size_t value_size, uint32_t flags, uint32_t exptime);

// 添加键值对（仅当key不存在时）
infra_error_t poly_memkv_add(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size, uint32_t flags, uint32_t exptime);

// 替换键值对（仅当key存在时）
infra_error_t poly_memkv_replace(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size, uint32_t flags, uint32_t exptime);

// 获取键值对
infra_error_t poly_memkv_get(poly_memkv_t* store, const char* key,
    poly_memkv_item_t** item);

// 删除键值对
infra_error_t poly_memkv_delete(poly_memkv_t* store, const char* key);

// 追加数据到现有值
infra_error_t poly_memkv_append(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size);

// 前置数据到现有值
infra_error_t poly_memkv_prepend(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size);

// CAS操作
infra_error_t poly_memkv_cas(poly_memkv_t* store, const char* key,
    const void* value, size_t value_size, uint32_t flags, uint32_t exptime,
    uint64_t cas);

// 获取统计信息
const poly_memkv_stats_t* poly_memkv_get_stats(poly_memkv_t* store);

// 清空存储
infra_error_t poly_memkv_flush(poly_memkv_t* store);

// 检查项目是否过期
bool poly_memkv_is_expired(const poly_memkv_item_t* item);

// 释放项目
void poly_memkv_free_item(poly_memkv_item_t* item);

// 增加值
infra_error_t poly_memkv_incr(poly_memkv_t* store, const char* key,
    uint64_t delta, uint64_t* new_value);

// 减少值
infra_error_t poly_memkv_decr(poly_memkv_t* store, const char* key,
    uint64_t delta, uint64_t* new_value);

#endif // POLY_MEMKV_H 