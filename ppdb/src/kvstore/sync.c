/**
 * @file sync.c
 * @brief PPDB 同步原语实现
 * 
 * 本文件实现了三种基本同步模式：
 * 1. 互斥锁模式：使用 pthread_mutex，适用于需要严格互斥的场景
 * 2. 自旋锁模式：使用原子操作，适用于锁竞争不激烈的场景
 * 3. 读写锁模式：使用 pthread_rwlock，适用于读多写少的场景
 * 
 * 性能优化说明：
 * - 自旋锁使用原子操作，避免线程切换开销
 * - 读写锁区分读写操作，提高并发度
 * - 提供哈希函数用于分片锁实现
 */

#include <cosmopolitan.h>
#include "internal/sync.h"

// MurmurHash2 , from previous codes, may useful later
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

// FNV-1a哈希函数实现
uint32_t ppdb_sync_hash(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t hash = 2166136261u;  // FNV offset basis
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;  // FNV prime
    }
    return hash;
}

/**
 * @brief 初始化同步原语
 * 
 * 根据配置类型初始化不同的同步原语：
 * - PPDB_SYNC_MUTEX: pthread_mutex_init
 * - PPDB_SYNC_SPINLOCK: atomic_flag_clear
 * - PPDB_SYNC_RWLOCK: pthread_rwlock_init
 * 
 * 注意：这些是基本的同步原语，分片锁优化在 sharded_memtable.c 中实现
 */
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config) {
    if (!sync || !config) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 清零同步原语结构
    memset(sync, 0, sizeof(ppdb_sync_t));

    // 设置同步类型
    sync->type = config->type;

    // 根据类型初始化
    int ret;
    switch (config->type) {
        case PPDB_SYNC_MUTEX: {
            pthread_mutexattr_t attr;
            ret = pthread_mutexattr_init(&attr);
            if (ret != 0) {
                return PPDB_ERR_MUTEX_ERROR;
            }
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            ret = pthread_mutex_init(&sync->mutex, &attr);
            pthread_mutexattr_destroy(&attr);
            if (ret != 0) {
                return PPDB_ERR_MUTEX_ERROR;
            }
            break;
        }
        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;
        case PPDB_SYNC_RWLOCK: {
            pthread_rwlockattr_t attr;
            ret = pthread_rwlockattr_init(&attr);
            if (ret != 0) {
                return PPDB_ERR_MUTEX_ERROR;
            }
            ret = pthread_rwlock_init(&sync->rwlock, &attr);
            pthread_rwlockattr_destroy(&attr);
            if (ret != 0) {
                return PPDB_ERR_MUTEX_ERROR;
            }
            break;
        }
        default:
            return PPDB_ERR_INVALID_ARG;
    }

    return PPDB_OK;
}

// 销毁同步原语
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    int ret = 0;
    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            ret = pthread_mutex_destroy(&sync->mutex);
            if (ret != 0) {
                return PPDB_ERR_MUTEX_ERROR;
            }
            break;
        case PPDB_SYNC_SPINLOCK:
            // 自旋锁不需要特殊销毁
            atomic_flag_clear(&sync->spinlock);
            break;
        case PPDB_SYNC_RWLOCK:
            ret = pthread_rwlock_destroy(&sync->rwlock);
            if (ret != 0) {
                return PPDB_ERR_MUTEX_ERROR;
            }
            break;
        default:
            return PPDB_ERR_INVALID_ARG;
    }

    // 清零同步原语结构
    memset(sync, 0, sizeof(ppdb_sync_t));
    return PPDB_OK;
}

// 尝试加锁
bool ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return false;
    }

    int ret;
    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            ret = pthread_mutex_trylock(&sync->mutex);
            return (ret == 0);
        case PPDB_SYNC_SPINLOCK:
            return !atomic_flag_test_and_set(&sync->spinlock);
        case PPDB_SYNC_RWLOCK:
            ret = pthread_rwlock_trywrlock(&sync->rwlock);
            return (ret == 0);
        default:
            return false;
    }
}

// 加锁
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    int ret;
    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            ret = pthread_mutex_lock(&sync->mutex);
            if (ret != 0) {
                return PPDB_ERR_MUTEX_ERROR;
            }
            break;
        case PPDB_SYNC_SPINLOCK:
            while (atomic_flag_test_and_set(&sync->spinlock)) {
                // 自旋等待
                microsleep(1);  // 短暂休眠以减少CPU占用
            }
            break;
        case PPDB_SYNC_RWLOCK:
            ret = pthread_rwlock_wrlock(&sync->rwlock);
            if (ret != 0) {
                return PPDB_ERR_MUTEX_ERROR;
            }
            break;
        default:
            return PPDB_ERR_INVALID_ARG;
    }

    return PPDB_OK;
}

// 解锁
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    int ret;
    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            ret = pthread_mutex_unlock(&sync->mutex);
            if (ret != 0) {
                return PPDB_ERR_MUTEX_ERROR;
            }
            break;
        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;
        case PPDB_SYNC_RWLOCK:
            ret = pthread_rwlock_unlock(&sync->rwlock);
            if (ret != 0) {
                return PPDB_ERR_MUTEX_ERROR;
            }
            break;
        default:
            return PPDB_ERR_INVALID_ARG;
    }

    return PPDB_OK;
}

// 文件同步函数
ppdb_error_t ppdb_sync_file(const char* filename) {
    if (!filename) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 打开文件
    int fd = open(filename, O_RDWR);
    if (fd < 0) {
        return PPDB_ERR_IO;
    }

    // 同步文件
    if (fsync(fd) != 0) {
        close(fd);
        return PPDB_ERR_IO;
    }

    // 关闭文件
    if (close(fd) != 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}
