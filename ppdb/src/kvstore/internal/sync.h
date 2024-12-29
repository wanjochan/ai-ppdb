#ifndef PPDB_KVSTORE_COMMON_SYNC_H_
#define PPDB_KVSTORE_COMMON_SYNC_H_

#include <cosmopolitan.h>
#include <stdatomic.h>

// 同步模式
typedef enum {
    PPDB_SYNC_MODE_MUTEX = 0,  // 互斥锁模式
    PPDB_SYNC_MODE_LOCKFREE = 1  // 无锁模式
} ppdb_sync_mode_t;

// 同步原语结构
typedef struct ppdb_sync_t {
    ppdb_sync_mode_t mode;  // 同步模式
    union {
        // 互斥锁模式
        struct {
            pthread_mutex_t* locks;  // 分段锁数组
            size_t count;           // 分段数量
        } mutex;
        
        // 无锁模式
        struct {
            _Atomic(void*)* slots;  // 原子指针数组
            size_t count;          // 槽位数量
        } lockfree;
    };
} ppdb_sync_t;

// 函数声明
// 初始化同步原语
void ppdb_sync_init(ppdb_sync_t* sync, ppdb_sync_mode_t mode, size_t count);

// 销毁同步原语
void ppdb_sync_destroy(ppdb_sync_t* sync);

// 加锁/进入临界区
void ppdb_sync_lock(ppdb_sync_t* sync, const void* key, size_t key_len);

// 解锁/离开临界区
void ppdb_sync_unlock(ppdb_sync_t* sync, const void* key, size_t key_len);

// 无锁操作: 比较并交换
bool ppdb_sync_cas(ppdb_sync_t* sync, const void* key, size_t key_len,
                  void* expected, void* desired);

// 无锁操作: 原子加载
void* ppdb_sync_load(ppdb_sync_t* sync, const void* key, size_t key_len);

// 无锁操作: 原子存储
void ppdb_sync_store(ppdb_sync_t* sync, const void* key, size_t key_len,
                    void* value);

#endif // PPDB_KVSTORE_COMMON_SYNC_H_
