#ifndef PPDB_KVSTORE_SYNC_H_
#define PPDB_KVSTORE_SYNC_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "ppdb/mutex.h"

// 节点状态
typedef enum ppdb_node_state {
    NODE_VALID = 0,      // 正常节点
    NODE_DELETED = 1,    // 已标记删除
    NODE_INSERTING = 2   // 正在插入
} ppdb_node_state_t;

// 引用计数
typedef struct ppdb_ref_count {
    atomic_uint count;   // 引用计数值
} ppdb_ref_count_t;

// 同步策略配置
typedef struct ppdb_sync_config {
    bool use_lockfree;        // 是否使用无锁模式
    uint32_t stripe_count;    // 分片锁数量（0表示不分片）
    uint32_t spin_count;      // 自旋次数
    uint32_t backoff_us;      // 退避时间(微秒)
    bool enable_ref_count;    // 是否启用引用计数
} ppdb_sync_config_t;

// 统一的同步原语
typedef struct ppdb_sync {
    union {
        atomic_int atomic;
        mutex_t mutex;
    } impl;
    ppdb_sync_config_t config;
    ppdb_ref_count_t* ref_count;  // 引用计数
    
    #ifdef PPDB_DEBUG
    struct {
        atomic_uint64_t contention_count;  // 竞争次数
        atomic_uint64_t wait_time_us;      // 等待时间
    } stats;
    #endif
} ppdb_sync_t;

// 分片锁管理器
typedef struct ppdb_stripe_locks {
    ppdb_sync_t* locks;       // 锁数组
    uint32_t count;           // 锁数量
    uint32_t mask;           // 分片掩码
} ppdb_stripe_locks_t;

// 基本同步API
void ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config);
void ppdb_sync_destroy(ppdb_sync_t* sync);
bool ppdb_sync_try_lock(ppdb_sync_t* sync);
void ppdb_sync_lock(ppdb_sync_t* sync);
void ppdb_sync_unlock(ppdb_sync_t* sync);

// 分片锁API
ppdb_stripe_locks_t* ppdb_stripe_locks_create(const ppdb_sync_config_t* config);
void ppdb_stripe_locks_destroy(ppdb_stripe_locks_t* locks);
bool ppdb_stripe_locks_try_lock(ppdb_stripe_locks_t* locks, const void* key, size_t key_len);
void ppdb_stripe_locks_lock(ppdb_stripe_locks_t* locks, const void* key, size_t key_len);
void ppdb_stripe_locks_unlock(ppdb_stripe_locks_t* locks, const void* key, size_t key_len);

// 引用计数API
void ppdb_ref_init(ppdb_ref_count_t* ref);
void ppdb_ref_inc(ppdb_ref_count_t* ref);
bool ppdb_ref_dec(ppdb_ref_count_t* ref);
uint32_t ppdb_ref_get(ppdb_ref_count_t* ref);

// 无锁原子操作API
bool ppdb_sync_cas(ppdb_sync_t* sync, void* expected, void* desired);
void* ppdb_sync_load(ppdb_sync_t* sync);
void ppdb_sync_store(ppdb_sync_t* sync, void* value);

// 节点状态API
bool ppdb_sync_is_valid(ppdb_sync_t* sync);
bool ppdb_sync_mark_deleted(ppdb_sync_t* sync);
bool ppdb_sync_mark_inserting(ppdb_sync_t* sync);

#endif // PPDB_KVSTORE_SYNC_H_
