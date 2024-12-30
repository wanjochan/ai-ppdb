#ifndef PPDB_SYNC_H
#define PPDB_SYNC_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"

// 同步类型枚举
typedef enum ppdb_sync_type {
    PPDB_SYNC_MUTEX,     // 互斥锁
    PPDB_SYNC_SPINLOCK,  // 自旋锁
    PPDB_SYNC_RWLOCK,    // 读写锁
    PPDB_SYNC_LOCKFREE   // 无锁
} ppdb_sync_type_t;

// 同步配置
typedef struct ppdb_sync_config {
    ppdb_sync_type_t type;     // 同步类型
    uint32_t spin_count;       // 自旋次数
} ppdb_sync_config_t;

// 同步原语
typedef struct ppdb_sync {
    ppdb_sync_type_t type;     // 同步类型
    union {
        pthread_mutex_t mutex;  // 互斥锁
        atomic_flag spinlock;   // 自旋锁
        struct {
            atomic_flag lock;   // 写锁
            atomic_uint readers; // 读者计数
        } rwlock;
    };
} ppdb_sync_t;

// 初始化同步原语
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config);

// 销毁同步原语
void ppdb_sync_destroy(ppdb_sync_t* sync);

// 加锁
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync);

// 尝试加锁
bool ppdb_sync_try_lock(ppdb_sync_t* sync);

// 解锁
void ppdb_sync_unlock(ppdb_sync_t* sync);

// 文件同步
ppdb_error_t ppdb_sync_file(int fd);

#endif // PPDB_SYNC_H
