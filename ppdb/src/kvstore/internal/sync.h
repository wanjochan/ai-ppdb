#ifndef PPDB_SYNC_H
#define PPDB_SYNC_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"

// 基本类型定义
typedef _Atomic(int) mutex_t;

// 同步类型
typedef enum ppdb_sync_type {
    PPDB_SYNC_TYPE_MUTEX,    // 互斥锁
    PPDB_SYNC_TYPE_SPINLOCK, // 自旋锁
    PPDB_SYNC_TYPE_RWLOCK    // 读写锁
} ppdb_sync_type_t;

// 同步配置
typedef struct ppdb_sync_config {
    ppdb_sync_type_t type;        // 同步类型
    bool use_lockfree;            // 是否使用无锁模式
    size_t stripe_count;          // 分片数量
    size_t spin_count;            // 自旋次数
    size_t yield_count;           // 让出次数
    size_t sleep_time;            // 睡眠时间(微秒)
    size_t backoff_us;            // 退避时间(微秒)
    bool enable_ref_count;        // 是否启用引用计数
} ppdb_sync_config_t;

// 同步原语
typedef struct ppdb_sync {
    ppdb_sync_type_t type;
    union {
        mutex_t mutex;
        atomic_flag spinlock;
        struct {
            mutex_t lock;
            atomic_int readers;
        } rwlock;
    };
} ppdb_sync_t;

// 函数声明
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config);
void ppdb_sync_destroy(ppdb_sync_t* sync);
void ppdb_sync_lock(ppdb_sync_t* sync);
void ppdb_sync_unlock(ppdb_sync_t* sync);
bool ppdb_sync_try_lock(ppdb_sync_t* sync);

// 文件同步函数
ppdb_error_t ppdb_sync_file(const char* filename);

// 哈希函数
uint32_t ppdb_sync_hash(const void* data, size_t len);

#endif // PPDB_SYNC_H
