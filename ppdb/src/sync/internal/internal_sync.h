#ifndef PPDB_INTERNAL_SYNC_H_
#define PPDB_INTERNAL_SYNC_H_

#include <pthread.h>
#include <stdatomic.h>
#include "ppdb/sync.h"

// 同步原语内部结构
struct ppdb_sync {
    ppdb_sync_type_t type;    // 同步类型
    bool use_lockfree;        // 是否使用无锁实现
    union {
        pthread_mutex_t mutex;     // 互斥锁
        atomic_flag spinlock;      // 自旋锁
        struct {
            atomic_int readers;    // 读者计数
            atomic_flag writer;    // 写者标志
        } rwlock;                  // 读写锁
    };
    int spin_count;           // 自旋次数
    int backoff_us;          // 退避时间(微秒)
    bool enable_ref_count;   // 是否启用引用计数
    atomic_int ref_count;    // 引用计数
};

#endif  // PPDB_INTERNAL_SYNC_H_ 