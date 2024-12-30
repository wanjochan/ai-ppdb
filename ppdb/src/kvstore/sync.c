#include <cosmopolitan.h>
#include <string.h>
#include <unistd.h>

// 公共头文件
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_logger.h"

// 内部头文件
#include "internal/kvstore_types.h"
#include "internal/sync.h"

// MurmurHash2 实现
static uint32_t murmur_hash2(const void* key, size_t len) {
    const uint32_t m = 0x5bd1e995;
    const uint32_t seed = 0x1234ABCD;
    const int r = 24;

    uint32_t h = seed ^ len;
    const unsigned char* data = (const unsigned char*)key;

    while (len >= 4) {
        uint32_t k = *(uint32_t*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    switch (len) {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0];
                h *= m;
    };

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

// 引用计数实现
void ppdb_ref_init(ppdb_ref_count_t* ref) {
    atomic_init(&ref->count, 1);
}

void ppdb_ref_inc(ppdb_ref_count_t* ref) {
    atomic_fetch_add(&ref->count, 1);
}

bool ppdb_ref_dec(ppdb_ref_count_t* ref) {
    uint32_t old = atomic_fetch_sub(&ref->count, 1);
    return old == 1;  // 返回是否应该释放
}

uint32_t ppdb_ref_get(ppdb_ref_count_t* ref) {
    return atomic_load(&ref->count);
}

void ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config) {
    memcpy(&sync->config, config, sizeof(ppdb_sync_config_t));
    
    if (config->use_lockfree) {
        atomic_init(&sync->impl.atomic, NODE_VALID);
    } else {
        mutex_init(&sync->impl.mutex);
    }
    
    if (config->enable_ref_count) {
        sync->ref_count = malloc(sizeof(ppdb_ref_count_t));
        if (sync->ref_count) {
            ppdb_ref_init(sync->ref_count);
        }
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
    
    if (sync->ref_count) {
        free(sync->ref_count);
    }
}

bool ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (sync->config.use_lockfree) {
        return atomic_compare_exchange_strong(&sync->impl.atomic, &(int){NODE_VALID}, NODE_LOCKED);
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
        while (!atomic_compare_exchange_strong(&sync->impl.atomic, &(int){NODE_VALID}, NODE_LOCKED)) {
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
        atomic_store(&sync->impl.atomic, NODE_VALID);
    } else {
        mutex_unlock(&sync->impl.mutex);
    }
}

// 节点状态API实现
bool ppdb_sync_is_valid(ppdb_sync_t* sync) {
    if (!sync->config.use_lockfree) {
        return true;
    }
    return atomic_load(&sync->impl.atomic) == NODE_VALID;
}

bool ppdb_sync_mark_deleted(ppdb_sync_t* sync) {
    if (!sync->config.use_lockfree) {
        return false;
    }
    int expected = NODE_VALID;
    return atomic_compare_exchange_strong(&sync->impl.atomic, 
                                       &expected, NODE_DELETED);
}

bool ppdb_sync_mark_inserting(ppdb_sync_t* sync) {
    if (!sync->config.use_lockfree) {
        return false;
    }
    int expected = NODE_VALID;
    return atomic_compare_exchange_strong(&sync->impl.atomic,
                                       &expected, NODE_INSERTING);
}

// 分片锁优化实现
static inline uint32_t optimize_stripe_count(uint32_t count) {
    // 确保分片数是2的幂
    uint32_t optimized = 1;
    while (optimized < count) {
        optimized <<= 1;
    }
    return optimized;
}

ppdb_stripe_locks_t* ppdb_stripe_locks_create(const ppdb_sync_config_t* config) {
    if (config->stripe_count == 0) {
        return NULL;
    }
    
    ppdb_stripe_locks_t* stripes = malloc(sizeof(ppdb_stripe_locks_t));
    if (!stripes) {
        return NULL;
    }
    
    // 优化分片数量
    uint32_t optimized_count = optimize_stripe_count(config->stripe_count);
    stripes->count = optimized_count;
    stripes->mask = optimized_count - 1;
    
    // 内存布局优化: 将锁对齐到缓存行
    size_t aligned_size = (sizeof(ppdb_sync_t) + 63) & ~63;
    stripes->locks = aligned_alloc(64, aligned_size * optimized_count);
    
    if (!stripes->locks) {
        free(stripes);
        return NULL;
    }
    
    for (uint32_t i = 0; i < optimized_count; i++) {
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
    return murmur_hash2(key, key_len) & stripes->mask;
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

// 无锁原子操作
bool ppdb_sync_cas(ppdb_sync_t* sync, void* expected, void* desired) {
    if (!sync->config.use_lockfree) {
        ppdb_log_error("CAS operation only supported in lockfree mode");
        return false;
    }
    return atomic_compare_exchange_strong(&sync->impl.atomic, expected, (int)desired);
}

void* ppdb_sync_load(ppdb_sync_t* sync) {
    if (!sync->config.use_lockfree) {
        ppdb_log_error("Atomic load only supported in lockfree mode");
        return NULL;
    }
    return (void*)atomic_load(&sync->impl.atomic);
}

void ppdb_sync_store(ppdb_sync_t* sync, void* value) {
    if (!sync->config.use_lockfree) {
        ppdb_log_error("Atomic store only supported in lockfree mode");
        return;
    }
    atomic_store(&sync->impl.atomic, (int)value);
}
