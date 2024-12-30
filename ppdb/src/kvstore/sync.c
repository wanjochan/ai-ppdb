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

// 初始化同步原语
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config) {
    if (!sync || !config) {
        return PPDB_ERR_INVALID_ARG;
    }

    sync->type = config->type;

    switch (config->type) {
        case PPDB_SYNC_TYPE_MUTEX:
            atomic_init(&sync->mutex, 0);
            break;
        case PPDB_SYNC_TYPE_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;
        case PPDB_SYNC_TYPE_RWLOCK:
            atomic_init(&sync->rwlock.lock, 0);
            atomic_init(&sync->rwlock.readers, 0);
            break;
        default:
            return PPDB_ERR_INVALID_ARG;
    }

    return PPDB_OK;
}

// 销毁同步原语
void ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) return;
    // 目前不需要特殊的清理操作
}

// 尝试加锁
bool ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) return false;

    switch (sync->type) {
        case PPDB_SYNC_TYPE_MUTEX: {
            int expected = 0;
            return atomic_compare_exchange_strong(&sync->mutex, &expected, 1);
        }
        case PPDB_SYNC_TYPE_SPINLOCK:
            return !atomic_flag_test_and_set(&sync->spinlock);
        case PPDB_SYNC_TYPE_RWLOCK: {
            int expected = 0;
            if (atomic_compare_exchange_strong(&sync->rwlock.lock, &expected, 1)) {
                while (atomic_load(&sync->rwlock.readers) > 0) {
                    // 等待读者完成
                }
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

// 加锁
void ppdb_sync_lock(ppdb_sync_t* sync) {
    if (!sync) return;

    switch (sync->type) {
        case PPDB_SYNC_TYPE_MUTEX:
            while (true) {
                int expected = 0;
                if (atomic_compare_exchange_strong(&sync->mutex, &expected, 1)) {
                    break;
                }
                usleep(1);  // 简单的退避策略
            }
            break;
        case PPDB_SYNC_TYPE_SPINLOCK:
            while (atomic_flag_test_and_set(&sync->spinlock)) {
                usleep(1);  // 简单的退避策略
            }
            break;
        case PPDB_SYNC_TYPE_RWLOCK:
            while (true) {
                int expected = 0;
                if (atomic_compare_exchange_strong(&sync->rwlock.lock, &expected, 1)) {
                    while (atomic_load(&sync->rwlock.readers) > 0) {
                        // 等待读者完成
                        usleep(1);
                    }
                    break;
                }
                usleep(1);  // 简单的退避策略
            }
            break;
    }
}

// 解锁
void ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) return;

    switch (sync->type) {
        case PPDB_SYNC_TYPE_MUTEX:
            atomic_store(&sync->mutex, 0);
            break;
        case PPDB_SYNC_TYPE_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;
        case PPDB_SYNC_TYPE_RWLOCK:
            atomic_store(&sync->rwlock.lock, 0);
            break;
    }
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
