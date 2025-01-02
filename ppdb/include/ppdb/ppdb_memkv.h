#ifndef PPDB_MEMKV_H_
#define PPDB_MEMKV_H_

#include <cosmopolitan.h>
#include "ppdb/base.h"
#include "ppdb/storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 注意：这是优化合并后的新版本 MemKV 接口。
 * 如果你在使用原有的 skiplist/memtable/sharded 构建分支，
 * 请使用 memkv.h 而不是这个文件。
 */

// MemKV配置选项
typedef struct {
    size_t memory_limit;      // 内存使用上限
    size_t shard_count;       // 分片数量
    uint32_t bloom_bits;      // Bloom过滤器位数
    bool enable_stats;        // 是否启用统计
    void* user_data;         // 用户数据
} ppdb_memkv_config_t;

// MemKV状态信息
typedef struct {
    size_t memory_used;      // 当前内存使用量
    size_t item_count;       // 当前存储的键值对数量
    struct {
        uint64_t get_count;   // Get操作次数
        uint64_t get_hits;    // Get命中次数
        uint64_t put_count;   // Put操作次数
        uint64_t del_count;   // Delete操作次数
    } stats;
} ppdb_memkv_status_t;

// 迭代器
typedef struct ppdb_memkv_iter_t ppdb_memkv_iter_t;

// 批量操作
typedef struct ppdb_memkv_batch_t ppdb_memkv_batch_t;

// 快照
typedef struct ppdb_memkv_snapshot_t ppdb_memkv_snapshot_t;

// 创建MemKV实例
ppdb_status_t ppdb_memkv_create(ppdb_base_t** base, const ppdb_memkv_config_t* config);

// 销毁MemKV实例
void ppdb_memkv_destroy(ppdb_base_t* base);

// 基本操作
ppdb_status_t ppdb_memkv_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_status_t ppdb_memkv_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_status_t ppdb_memkv_delete(ppdb_base_t* base, const ppdb_key_t* key);

// 批量操作
ppdb_memkv_batch_t* ppdb_memkv_batch_create(ppdb_base_t* base);
void ppdb_memkv_batch_destroy(ppdb_memkv_batch_t* batch);
ppdb_status_t ppdb_memkv_batch_put(ppdb_memkv_batch_t* batch, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_status_t ppdb_memkv_batch_delete(ppdb_memkv_batch_t* batch, const ppdb_key_t* key);
ppdb_status_t ppdb_memkv_batch_commit(ppdb_memkv_batch_t* batch);

// 迭代器
ppdb_memkv_iter_t* ppdb_memkv_iter_create(ppdb_base_t* base);
void ppdb_memkv_iter_destroy(ppdb_memkv_iter_t* iter);
void ppdb_memkv_iter_seek_first(ppdb_memkv_iter_t* iter);
void ppdb_memkv_iter_seek_last(ppdb_memkv_iter_t* iter);
void ppdb_memkv_iter_seek(ppdb_memkv_iter_t* iter, const ppdb_key_t* key);
void ppdb_memkv_iter_next(ppdb_memkv_iter_t* iter);
void ppdb_memkv_iter_prev(ppdb_memkv_iter_t* iter);
bool ppdb_memkv_iter_valid(const ppdb_memkv_iter_t* iter);
const ppdb_key_t* ppdb_memkv_iter_key(const ppdb_memkv_iter_t* iter);
const ppdb_value_t* ppdb_memkv_iter_value(const ppdb_memkv_iter_t* iter);

// 快照
ppdb_memkv_snapshot_t* ppdb_memkv_snapshot_create(ppdb_base_t* base);
void ppdb_memkv_snapshot_destroy(ppdb_memkv_snapshot_t* snapshot);
ppdb_status_t ppdb_memkv_snapshot_get(ppdb_memkv_snapshot_t* snapshot, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_memkv_iter_t* ppdb_memkv_snapshot_iter_create(ppdb_memkv_snapshot_t* snapshot);

// 状态查询
ppdb_status_t ppdb_memkv_get_status(ppdb_base_t* base, ppdb_memkv_status_t* status);

// 维护操作
ppdb_status_t ppdb_memkv_compact(ppdb_base_t* base);
ppdb_status_t ppdb_memkv_clear(ppdb_base_t* base);

#ifdef __cplusplus
}
#endif

#endif // PPDB_MEMKV_H_
