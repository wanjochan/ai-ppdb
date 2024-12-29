#ifndef PPDB_KVSTORE_INTERNAL_SYNC_H
#define PPDB_KVSTORE_INTERNAL_SYNC_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"

// 同步原语结构
typedef struct ppdb_sync {
    mutex_t mutex;
    atomic_bool is_locked;
} ppdb_sync_t;

// 初始化同步原语
void ppdb_sync_init(ppdb_sync_t* sync);

// 销毁同步原语
void ppdb_sync_destroy(ppdb_sync_t* sync);

// 加锁
void ppdb_sync_lock(ppdb_sync_t* sync);

// 解锁
void ppdb_sync_unlock(ppdb_sync_t* sync);

// 尝试加锁
bool ppdb_sync_try_lock(ppdb_sync_t* sync);

// 是否已锁定
bool ppdb_sync_is_locked(ppdb_sync_t* sync);

#endif // PPDB_KVSTORE_INTERNAL_SYNC_H
