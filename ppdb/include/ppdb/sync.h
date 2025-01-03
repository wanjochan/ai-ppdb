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

#include "cosmopolitan.h"

// 错误码定义
typedef enum {
    PPDB_OK = 0,
    PPDB_ERR_NULL_POINTER,
    PPDB_ERR_INVALID_STATE,
    PPDB_ERR_BUSY,
    PPDB_ERR_TIMEOUT,
    PPDB_ERR_TOO_MANY_READERS,
    PPDB_ERR_NOT_SUPPORTED,
    PPDB_ERR_INTERNAL,
    PPDB_ERR_OUT_OF_MEMORY,
    PPDB_ERR_UNLOCK_FAILED,
    PPDB_ERR_RETRY,
    PPDB_ERR_SYNC_RETRY_FAILED
} ppdb_error_t;

typedef enum {
    PPDB_SYNC_MUTEX,
    PPDB_SYNC_SPINLOCK,
    PPDB_SYNC_RWLOCK
} ppdb_sync_type_t;

// 重试函数类型定义
typedef ppdb_error_t (*ppdb_sync_retry_func_t)(void*);

typedef struct ppdb_sync_config {
    ppdb_sync_type_t type;
    bool use_lockfree;
    bool enable_ref_count;
    uint32_t max_readers;
    uint32_t backoff_us;
    uint32_t max_retries;
} ppdb_sync_config_t;

typedef struct ppdb_sync_stats {
    atomic_uint_least64_t read_locks;
    atomic_uint_least64_t write_locks;
    atomic_uint_least64_t read_timeouts;
    atomic_uint_least64_t write_timeouts;
    atomic_uint_least64_t contentions;
} ppdb_sync_stats_t;

typedef struct ppdb_rwlock {
    atomic_int state;
    atomic_int waiters;
} ppdb_rwlock_t;

typedef struct ppdb_sync {
    ppdb_sync_config_t config;
    ppdb_sync_stats_t stats;
    union {
        pthread_mutex_t mutex;
        atomic_flag spinlock;
        ppdb_rwlock_t rwlock;
    };
} ppdb_sync_t;

// 函数声明
ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, ppdb_sync_config_t* config);
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, ppdb_sync_config_t* config);
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_read_lock_shared(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock_shared(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_retry(ppdb_sync_t* sync, ppdb_sync_retry_func_t retry_func, void* arg);

#endif // PPDB_SYNC_H 