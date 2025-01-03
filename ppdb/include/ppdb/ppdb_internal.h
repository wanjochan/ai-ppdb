#ifndef PPDB_INTERNAL_H
#define PPDB_INTERNAL_H

#include "ppdb/ppdb.h"
#include <cosmopolitan.h>

// 内部同步原语实现
#ifdef PPDB_SYNC_MODE_LOCKFREE
#define PPDB_SYNC_USE_LOCKFREE 1
#else
#define PPDB_SYNC_USE_LOCKFREE 0
#endif

// 内部常量定义
#define PPDB_MAX_RETRY_COUNT 100
#define PPDB_RETRY_DELAY_US 1
#define PPDB_MAX_READERS 1024
#define PPDB_SPIN_COUNT 1000

// 内部类型定义
typedef struct ppdb_sync_internal {
    ppdb_sync_config_t config;
    ppdb_sync_stats_t stats;
    union {
        pthread_mutex_t mutex;
        atomic_flag spinlock;
        struct {
            atomic_int readers;
            atomic_flag write_lock;
        } rwlock;
    };
} ppdb_sync_internal_t;

// 内部函数声明
static inline void ppdb_sync_pause(void) {
    __asm__ volatile("pause");
}

static inline void ppdb_sync_yield(void) {
    sched_yield();
}

static inline void ppdb_sync_backoff(uint32_t attempts) {
    if (attempts < 10) {
        ppdb_sync_pause();
    } else if (attempts < 20) {
        for (int i = 0; i < attempts; i++) {
            ppdb_sync_pause();
        }
    } else if (attempts < 30) {
        ppdb_sync_yield();
    } else {
        usleep(1);
    }
}

#endif // PPDB_INTERNAL_H 