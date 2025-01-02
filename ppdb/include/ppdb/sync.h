/**
 * @file sync.h
 * @brief PPDB 同步原语公共接口
 * 
 * 本模块提供了一套完整的同步原语接口，支持以下功能：
 * 1. 多种同步模式：
 *    - 互斥锁 (PPDB_SYNC_MUTEX)：基于原子操作，适用于普通的互斥场景
 *    - 自旋锁 (PPDB_SYNC_SPINLOCK)：基于自旋，适用于短期锁定场景
 *    - 读写锁 (PPDB_SYNC_RWLOCK)：支持读写分离，适用于读多写少场景
 *    - 共享锁 (PPDB_SYNC_SHARED)：新增的共享锁模式
 * 
 * 2. 高级特性：
 *    - 无锁操作：支持 CAS 和原子操作
 *    - 重试机制：可配置的重试策略
 *    - 性能优化：支持自旋和退避
 * 
 * 3. 配置选项：
 *    - 同步模式选择
 *    - 重试参数设置
 *    - 性能参数优化
 *    - 最大读者数量限制
 *    - 公平调度标志
 * 
 * 4. 计划中的特性：
 *    - 分片优化：通过哈希分片减少锁竞争（开发中）
 *    - 条件变量支持
 *    - 死锁检测
 *    - 性能统计
 * 
 * 使用示例：
 * @code
 * // 创建同步对象
 * ppdb_sync_t* sync = ppdb_sync_create();
 * 
 * // 配置同步选项
 * ppdb_sync_config_t config = {
 *     .type = PPDB_SYNC_MUTEX,
 *     .use_lockfree = true,
 *     .retry_count = 100,
 *     .retry_delay_us = 1
 * };
 * 
 * // 初始化
 * ppdb_sync_init(sync, &config);
 * 
 * // 使用
 * while (ppdb_sync_try_lock(sync) == PPDB_ERR_BUSY) {
 *     __asm__ volatile("pause");
 * }
 * // ... 临界区操作 ...
 * ppdb_sync_unlock(sync);
 * 
 * // 清理
 * ppdb_sync_destroy(sync);
 * @endcode
 * 
 * @note 分片锁功能目前正在开发中。虽然配置结构中包含了 stripe_count 参数，
 *       但实际的分片逻辑尚未实现。在未来版本中，这将用于实现更细粒度的锁控制，
 *       以减少锁竞争并提高并发性能。
 */

#ifndef PPDB_SYNC_H
#define PPDB_SYNC_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"

// 同步原语类型
typedef enum ppdb_sync_type {
    PPDB_SYNC_MUTEX,    // 互斥锁
    PPDB_SYNC_SPINLOCK, // 自旋锁
    PPDB_SYNC_RWLOCK    // 读写锁
} ppdb_sync_type_t;

// 读写锁结构
typedef struct ppdb_rwlock {
    atomic_int readers;          // 当前读者数量
    atomic_int waiting_writers;  // 等待的写者数量
    atomic_flag writer;          // 写者标志
    atomic_int atomic_lock;      // 原子锁
} ppdb_rwlock_t;

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
    ppdb_sync_type_t type;      // 同步原语类型
    bool use_lockfree;          // 是否使用无锁模式
    bool enable_fairness;       // 是否启用公平性
    bool enable_ref_count;      // 是否启用引用计数
    uint32_t spin_count;        // 自旋次数
    uint32_t backoff_us;        // 退避时间（微秒）
    uint32_t max_readers;       // 最大读者数量

    union {
        pthread_mutex_t mutex;   // 互斥锁
        atomic_flag spinlock;    // 自旋锁
        ppdb_rwlock_t rwlock;   // 读写锁
    };
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