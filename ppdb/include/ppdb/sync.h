/**
 * @file sync.h
 * @brief PPDB 同步原语公共接口
 * 
 * 本模块提供了一套完整的同步原语接口，支持以下功能：
 * 1. 多种同步模式：
 *    - 互斥锁 (PPDB_SYNC_MUTEX)：基于原子操作，适用于普通的互斥场景
 *    - 自旋锁 (PPDB_SYNC_SPINLOCK)：基于自旋，适用于短期锁定场景
 *    - 读写锁 (PPDB_SYNC_RWLOCK)：支持读写分离，适用于读多写少场景
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
 * ppdb_sync_lock(sync);
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 同步类型枚举
 */
typedef enum ppdb_sync_type {
    PPDB_SYNC_MUTEX,    ///< 互斥锁模式
    PPDB_SYNC_SPINLOCK, ///< 自旋锁模式
    PPDB_SYNC_RWLOCK    ///< 读写锁模式
} ppdb_sync_type_t;

/**
 * @brief 同步配置结构
 */
typedef struct ppdb_sync_config {
    ppdb_sync_type_t type;      ///< 同步类型
    bool use_lockfree;          ///< 是否使用无锁模式
    size_t stripe_count;        ///< 分片数量
    uint32_t spin_count;        ///< 自旋次数
    uint32_t backoff_us;        ///< 退避时间
    bool enable_ref_count;      ///< 是否启用引用计数
    uint32_t retry_count;       ///< 最大重试次数
    uint32_t retry_delay_us;    ///< 重试间隔
} ppdb_sync_config_t;

/**
 * @brief 默认同步配置
 */
#define PPDB_SYNC_CONFIG_DEFAULT {      \
    .type = PPDB_SYNC_MUTEX,           \
    .use_lockfree = false,             \
    .stripe_count = 16,                \
    .spin_count = 10000,               \
    .backoff_us = 100,                 \
    .enable_ref_count = true,          \
    .retry_count = 100,                \
    .retry_delay_us = 1                \
}

/**
 * @brief 创建同步原语对象
 * @return 新创建的同步原语对象，失败返回 NULL
 */
ppdb_sync_t* ppdb_sync_create(void);

/**
 * @brief 初始化同步原语
 * @param sync 同步原语对象
 * @param config 配置参数
 * @return PPDB_OK 成功，其他值表示错误
 */
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config);

/**
 * @brief 销毁同步原语
 * @param sync 同步原语对象
 * @return PPDB_OK 成功，其他值表示错误
 */
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync);

/**
 * @brief 加锁操作
 * @param sync 同步原语对象
 * @return PPDB_OK 成功，其他值表示错误
 */
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync);

/**
 * @brief 尝试加锁
 * @param sync 同步原语对象
 * @return true 加锁成功，false 加锁失败
 */
bool ppdb_sync_try_lock(ppdb_sync_t* sync);

/**
 * @brief 解锁操作
 * @param sync 同步原语对象
 * @return PPDB_OK 成功，其他值表示错误
 */
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);

/**
 * @brief 读锁加锁操作
 * @param sync 同步原语对象
 * @return PPDB_OK 成功，其他值表示错误
 */
ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync);

/**
 * @brief 读锁解锁操作
 * @param sync 同步原语对象
 * @return PPDB_OK 成功，其他值表示错误
 */
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync);

/**
 * @brief 无锁操作接口
 */
ppdb_error_t ppdb_sync_lockfree_put(ppdb_sync_t* sync, void* key, size_t key_len,
                                   void* value, size_t value_len,
                                   ppdb_sync_config_t* config);

ppdb_error_t ppdb_sync_lockfree_get(ppdb_sync_t* sync, void* key, size_t key_len,
                                   void** value, size_t* value_len,
                                   ppdb_sync_config_t* config);

ppdb_error_t ppdb_sync_lockfree_delete(ppdb_sync_t* sync, void* key, size_t key_len,
                                      ppdb_sync_config_t* config);

/**
 * @brief 文件同步函数
 * @param filename 文件名
 * @return PPDB_OK 成功，其他值表示错误
 */
ppdb_error_t ppdb_sync_file(const char* filename);

/**
 * @brief 文件描述符同步函数
 * @param fd 文件描述符
 * @return PPDB_OK 成功，其他值表示错误
 */
ppdb_error_t ppdb_sync_fd(int fd);

/**
 * @brief 哈希函数
 * @param data 数据指针
 * @param len 数据长度
 * @return 32位哈希值
 */
uint32_t ppdb_sync_hash(const void* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // PPDB_SYNC_H 