#include "sync.h"
#include <string.h>
#include <unistd.h>
#include "ppdb/hash.h"

void ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config) {
    memcpy(&sync->config, config, sizeof(ppdb_sync_config_t));
    
    if (config->use_lockfree) {
        atomic_init(&sync->impl.atomic, 0);
    } else {
        mutex_init(&sync->impl.mutex);
    }
    
    #ifdef PPDB_DEBUG
    atomic_init(&sync->stats.contention_count, 0);
    atomic_init(&sync->stats.wait_time_us, 0);
    #endif
}

void ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync->config.use_lockfree) {
        mutex_destroy(&sync->impl.mutex);
    }
}

bool ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (sync->config.use_lockfree) {
        return atomic_compare_exchange_strong(&sync->impl.atomic, &(int){0}, 1);
    } else {
        return mutex_trylock(&sync->impl.mutex) == 0;
    }
}

void ppdb_sync_lock(ppdb_sync_t* sync) {
    uint32_t attempts = 0;
    
    #ifdef PPDB_DEBUG
    uint64_t start_time = get_current_time_us();
    #endif
    
    while (attempts++ < sync->config.spin_count) {
        if (ppdb_sync_try_lock(sync)) {
            return;
        }
        
        if (sync->config.backoff_us > 0) {
            usleep(sync->config.backoff_us);
        }
    }
    
    // 自旋失败后强制获取锁
    if (sync->config.use_lockfree) {
        while (!atomic_compare_exchange_strong(&sync->impl.atomic, &(int){0}, 1)) {
            usleep(sync->config.backoff_us);
        }
    } else {
        mutex_lock(&sync->impl.mutex);
    }
    
    #ifdef PPDB_DEBUG
    atomic_fetch_add(&sync->stats.contention_count, 1);
    atomic_fetch_add(&sync->stats.wait_time_us, 
                    get_current_time_us() - start_time);
    #endif
}

void ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (sync->config.use_lockfree) {
        atomic_store(&sync->impl.atomic, 0);
    } else {
        mutex_unlock(&sync->impl.mutex);
    }
}

// 分片锁实现
ppdb_stripe_locks_t* ppdb_stripe_locks_create(const ppdb_sync_config_t* config) {
    if (config->stripe_count == 0) {
        return NULL;
    }
    
    ppdb_stripe_locks_t* stripes = malloc(sizeof(ppdb_stripe_locks_t));
    if (!stripes) {
        return NULL;
    }
    
    stripes->count = config->stripe_count;
    stripes->mask = config->stripe_count - 1;  // 必须是2的幂
    stripes->locks = calloc(config->stripe_count, sizeof(ppdb_sync_t));
    
    if (!stripes->locks) {
        free(stripes);
        return NULL;
    }
    
    for (uint32_t i = 0; i < config->stripe_count; i++) {
        ppdb_sync_init(&stripes->locks[i], config);
    }
    
    return stripes;
}

void ppdb_stripe_locks_destroy(ppdb_stripe_locks_t* stripes) {
    if (!stripes) return;
    
    for (uint32_t i = 0; i < stripes->count; i++) {
        ppdb_sync_destroy(&stripes->locks[i]);
    }
    
    free(stripes->locks);
    free(stripes);
}

static inline uint32_t get_stripe_index(ppdb_stripe_locks_t* stripes,
                                      const void* key, size_t key_len) {
    return ppdb_hash(key, key_len) & stripes->mask;
}

bool ppdb_stripe_locks_try_lock(ppdb_stripe_locks_t* stripes,
                               const void* key, size_t key_len) {
    uint32_t idx = get_stripe_index(stripes, key, key_len);
    return ppdb_sync_try_lock(&stripes->locks[idx]);
}

void ppdb_stripe_locks_lock(ppdb_stripe_locks_t* stripes,
                           const void* key, size_t key_len) {
    uint32_t idx = get_stripe_index(stripes, key, key_len);
    ppdb_sync_lock(&stripes->locks[idx]);
}

void ppdb_stripe_locks_unlock(ppdb_stripe_locks_t* stripes,
                            const void* key, size_t key_len) {
    uint32_t idx = get_stripe_index(stripes, key, key_len);
    ppdb_sync_unlock(&stripes->locks[idx]);
}
