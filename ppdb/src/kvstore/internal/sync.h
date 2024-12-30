/**
 * @file sync.h
 * @brief PPDB 同步原语实现
 * 
 * 本模块提供统一的同步原语接口，支持三种基本同步模式：
 * 1. 互斥锁模式 (PPDB_SYNC_MUTEX)：基于 pthread_mutex，适用于普通的互斥场景
 * 2. 自旋锁模式 (PPDB_SYNC_SPINLOCK)：基于原子操作，适用于短期锁定场景
 * 3. 读写锁模式 (PPDB_SYNC_RWLOCK)：基于 pthread_rwlock，适用于读多写少场景
 * 
 * 注意：分片锁不是一种独立的同步原语模式，而是在这些基本模式之上的优化策略。
 * 分片锁的实现位于 sharded_memtable.c，用于减少锁竞争和提高并发性能。
 */

#ifndef PPDB_SYNC_H
#define PPDB_SYNC_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"

/**
 * @brief 初始化同步原语
 * @param sync 同步原语对象
 * @param config 配置参数，包含同步模式类型
 * @return PPDB_OK 成功，其他值表示错误
 * 
 * 根据配置参数初始化不同类型的同步原语：
 * - PPDB_SYNC_MUTEX: 初始化互斥锁
 * - PPDB_SYNC_SPINLOCK: 初始化自旋锁
 * - PPDB_SYNC_RWLOCK: 初始化读写锁
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
 * 
 * 不同模式的加锁行为：
 * - 互斥锁：阻塞等待直到获取锁
 * - 自旋锁：自旋等待直到获取锁
 * - 读写锁：以写模式加锁
 */
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync);

/**
 * @brief 尝试加锁
 * @param sync 同步原语对象
 * @return true 加锁成功，false 加锁失败
 * 
 * 非阻塞的加锁尝试，如果无法立即获取锁则返回失败
 */
bool ppdb_sync_try_lock(ppdb_sync_t* sync);

/**
 * @brief 解锁操作
 * @param sync 同步原语对象
 * @return PPDB_OK 成功，其他值表示错误
 */
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);

/**
 * @brief 同步文件到磁盘
 * @param filename 文件名
 * @return PPDB_OK 成功，其他值表示错误
 */
ppdb_error_t ppdb_sync_file(const char* filename);

/**
 * @brief 同步文件描述符到磁盘
 * @param fd 文件描述符
 * @return PPDB_OK 成功，其他值表示错误
 */
ppdb_error_t ppdb_sync_fd(int fd);

/**
 * @brief 哈希函数，用于分片锁的分片计算
 * @param data 数据指针
 * @param len 数据长度
 * @return 32位哈希值
 */
uint32_t ppdb_sync_hash(const void* data, size_t len);

#endif // PPDB_SYNC_H
