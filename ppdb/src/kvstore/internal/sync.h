#ifndef PPDB_KVSTORE_COMMON_SYNC_H_
#define PPDB_KVSTORE_COMMON_SYNC_H_

#include <cosmopolitan.h>

// 同步原语结构
typedef struct ppdb_stripe_locks_t {
    pthread_mutex_t* locks;
    size_t count;
} ppdb_stripe_locks_t;

// 函数声明
void ppdb_stripe_locks_init(ppdb_stripe_locks_t* locks, size_t count);
void ppdb_stripe_locks_destroy(ppdb_stripe_locks_t* locks);
void ppdb_stripe_locks_lock(ppdb_stripe_locks_t* locks, const void* key, size_t key_len);
void ppdb_stripe_locks_unlock(ppdb_stripe_locks_t* locks, const void* key, size_t key_len);

#endif // PPDB_KVSTORE_COMMON_SYNC_H_
