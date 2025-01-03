#ifndef PPDB_SYNC_H
#define PPDB_SYNC_H

#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "sync/internal/internal_sync.h"

// 同步原语类型
typedef enum ppdb_sync_type {
    PPDB_SYNC_MUTEX,    // 互斥锁
    PPDB_SYNC_SPINLOCK, // 自旋锁
    PPDB_SYNC_RWLOCK    // 读写锁
} ppdb_sync_type_t;

// 同步配置
typedef struct ppdb_sync_config {
    ppdb_sync_type_t type;      // 同步原语类型
    bool use_lockfree;          // 是否使用无锁模式
    bool enable_fairness;       // 是否启用公平性
    bool enable_ref_count;      // 是否启用引用计数
    uint32_t spin_count;        // 自旋次数
    uint32_t backoff_us;        // 退避时间（微秒）
    uint32_t max_readers;       // 最大读者数量
} ppdb_sync_config_t;

// 同步原语结构
typedef struct ppdb_sync {
    sync_t base;                // 基础同步结构
    ppdb_sync_type_t type;      // 同步原语类型
    bool use_lockfree;          // 是否使用无锁模式
    bool enable_fairness;       // 是否启用公平性
    bool enable_ref_count;      // 是否启用引用计数
    uint32_t spin_count;        // 自旋次数
    uint32_t backoff_us;        // 退避时间（微秒）
    uint32_t max_readers;       // 最大读者数量
} ppdb_sync_t;

// 无锁操作参数结构
typedef struct ppdb_sync_lockfree_args {
    ppdb_sync_t* sync;          // 同步原语
    void* key;                  // 键
    size_t key_len;            // 键长度
    void* value;               // 值
    void* value_ptr;           // 值指针
    size_t value_len;          // 值长度
} ppdb_sync_lockfree_args_t;

// 重试函数类型
typedef ppdb_error_t (*ppdb_sync_retry_func_t)(void*);

// 基本同步操作
ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, ppdb_sync_config_t* config);
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, ppdb_sync_config_t* config);
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync);

// 锁操作
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);

// 读写锁操作
ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync);

// 共享读锁操作
ppdb_error_t ppdb_sync_read_lock_shared(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock_shared(ppdb_sync_t* sync);

// 无锁操作
ppdb_error_t ppdb_sync_lockfree_put(ppdb_sync_t* sync, void* key, size_t key_len, void* value, size_t value_len);
ppdb_error_t ppdb_sync_lockfree_get(ppdb_sync_t* sync, void* key, size_t key_len, void* value, size_t value_len);
ppdb_error_t ppdb_sync_lockfree_delete(ppdb_sync_t* sync, void* key, size_t key_len);

// 重试机制
ppdb_error_t ppdb_sync_retry(ppdb_sync_t* sync, ppdb_sync_retry_func_t retry_func, void* arg);

#endif // PPDB_SYNC_H 