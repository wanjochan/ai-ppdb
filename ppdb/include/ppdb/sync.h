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

#ifndef PPDB_SYNC_H_
#define PPDB_SYNC_H_

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"

// 同步原语类型
typedef enum {
    PPDB_SYNC_MUTEX = 0,  // 互斥锁
    PPDB_SYNC_SPINLOCK,   // 自旋锁
    PPDB_SYNC_RWLOCK,     // 读写锁
    PPDB_SYNC_SHARED      // 新增：共享锁模式
} ppdb_sync_type_t;

// 默认配置
#define PPDB_SYNC_CONFIG_DEFAULT { \
    .type = PPDB_SYNC_MUTEX,      \
    .spin_count = 1000,           \
    .use_lockfree = false,        \
    .stripe_count = 1,            \
    .backoff_us = 1,              \
    .enable_ref_count = false,    \
    .retry_count = 100,           \
    .retry_delay_us = 1,          \
    .max_readers = 32,            \
    .enable_fairness = true       \
}

/**
 * @brief 同步原语配置
 * 
 * 这些配置参数直接决定了锁的性能和行为特征：
 * 
 * - spin_count: 自旋计数阈值，决定了在进入睡眠前的自旋次数
 *   - 值越大，CPU占用越高，但在高竞争下响应更快
 *   - 值越小，CPU占用越低，但可能增加上下文切换
 * 
 * - backoff_us: 退避睡眠时间（微秒）
 *   - 值越大，CPU占用越低，但响应延迟增加
 *   - 值越小，响应更快，但在高竞争下可能导致CPU占用过高
 * 
 * - retry_count: 重试次数，超过后返回PPDB_ERR_BUSY
 *   - 值越大，等待时间越长，但成功概率增加
 *   - 值越小，快速失败，但可能需要上层重试
 * 
 * - retry_delay_us: 重试间隔（微秒）
 *   - 值越大，CPU占用越低，但响应延迟增加
 *   - 值越小，响应更快，但可能增加竞争
 */
typedef struct {
    ppdb_sync_type_t type;        ///< 同步原语类型
    bool use_lockfree;            ///< 是否使用无锁模式
    uint32_t stripe_count;        ///< 分片数量（0表示不分片）
    uint32_t spin_count;          ///< 自旋计数阈值
    uint32_t backoff_us;          ///< 退避睡眠时间（微秒）
    bool enable_ref_count;        ///< 是否启用引用计数
    uint32_t retry_count;         ///< 重试次数
    uint32_t retry_delay_us;      ///< 重试间隔（微秒）
    uint32_t max_readers;         ///< 新增：最大读者数量限制
    bool enable_fairness;         ///< 新增：是否启用公平调度
} ppdb_sync_config_t;

// 同步原语结构
struct ppdb_sync {
    ppdb_sync_type_t type;    // 同步类型
    bool use_lockfree;        // 是否使用无锁实现
    union {
        pthread_mutex_t mutex;     // 互斥锁
        atomic_flag spinlock;      // 自旋锁
        struct {
            atomic_int readers;    // 读者计数
            atomic_flag writer;    // 写者标志
            atomic_int waiting_writers; // 等待的写者数量
            atomic_int waiting_readers; // 等待的读者数量
            atomic_uint atomic_lock;   // 无锁模式的原子锁
        } rwlock;                  // 读写锁
    };
    atomic_int ref_count;      // 引用计数
    
    // 配置参数
    uint32_t spin_count;      // 自旋次数
    uint32_t backoff_us;      // 退避时间(微秒)
    bool enable_ref_count;    // 是否启用引用计数
    uint32_t max_readers;     // 最大读者数量限制
    bool enable_fairness;     // 公平调度标志
    atomic_int total_waiters; // 总等待者计数
    atomic_flag is_contended; // 竞争状态标志
};

typedef struct ppdb_sync ppdb_sync_t;

// 基本同步操作
ppdb_sync_t* ppdb_sync_create(void);
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config);
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync);

// 锁操作
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync);

// 共享锁操作
ppdb_error_t ppdb_sync_read_lock_shared(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock_shared(ppdb_sync_t* sync);

// 无锁操作
ppdb_error_t ppdb_sync_lockfree_put(ppdb_sync_t* sync,
                                   void* key,
                                   size_t key_len,
                                   void* value,
                                   size_t value_len,
                                   ppdb_sync_config_t* config);

ppdb_error_t ppdb_sync_lockfree_get(ppdb_sync_t* sync,
                                   void* key,
                                   size_t key_len,
                                   void** value,
                                   size_t* value_len,
                                   ppdb_sync_config_t* config);

ppdb_error_t ppdb_sync_lockfree_delete(ppdb_sync_t* sync,
                                      void* key,
                                      size_t key_len,
                                      ppdb_sync_config_t* config);

// 工具函数
uint32_t ppdb_sync_hash(const void* data, size_t len);

#endif  // PPDB_SYNC_H_ 