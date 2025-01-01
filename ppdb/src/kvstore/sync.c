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
#include "kvstore/internal/sync.h"
#include "ppdb/ppdb_logger.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_error.h"

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
    switch (config->type) {
        case PPDB_SYNC_MUTEX: {
            sync->mutex = 0;  // 初始化为未锁定状态
            break;
        }
        case PPDB_SYNC_SPINLOCK:
            sync->spinlock = 0;  // 初始化为未锁定状态
            break;
        case PPDB_SYNC_RWLOCK: {
            sync->rwlock.readers = 0;  // 初始化读者数量
            sync->rwlock.writer = 0;   // 初始化写者标志
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

    // 清零同步原语结构
    memset(sync, 0, sizeof(ppdb_sync_t));
    return PPDB_OK;
}

// 尝试加锁
bool ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return false;
    }

    switch (sync->type) {
        case PPDB_SYNC_MUTEX: {
            int expected = 0;
            return __atomic_compare_exchange_n(&sync->mutex, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        }
        case PPDB_SYNC_SPINLOCK: {
            int expected = 0;
            return __atomic_compare_exchange_n(&sync->spinlock, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        }
        case PPDB_SYNC_RWLOCK: {
            // 尝试获取写锁
            if (__atomic_load_n(&sync->rwlock.readers, __ATOMIC_SEQ_CST) != 0) {
                return false;
            }
            int expected = 0;
            return __atomic_compare_exchange_n(&sync->rwlock.writer, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        }
        default:
            return false;
    }
}

// 加锁
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync) {
    if (!sync) {
        return PPDB_ERR_INVALID_ARG;
    }

    switch (sync->type) {
        case PPDB_SYNC_MUTEX: {
            while (true) {
                int expected = 0;
                if (__atomic_compare_exchange_n(&sync->mutex, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                    break;
                }
                usleep(1);  // 短暂休眠以减少CPU占用
            }
            break;
        }
        case PPDB_SYNC_SPINLOCK: {
            while (true) {
                int expected = 0;
                if (__atomic_compare_exchange_n(&sync->spinlock, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                    break;
                }
                usleep(1);  // 短暂休眠以减少CPU占用
            }
            break;
        }
        case PPDB_SYNC_RWLOCK: {
            // 等待所有读者完成
            while (__atomic_load_n(&sync->rwlock.readers, __ATOMIC_SEQ_CST) != 0) {
                usleep(1);
            }
            // 获取写锁
            while (true) {
                int expected = 0;
                if (__atomic_compare_exchange_n(&sync->rwlock.writer, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                    break;
                }
                usleep(1);
            }
            break;
        }
        default:
            return PPDB_ERR_INVALID_ARG;
    }

    return PPDB_OK;
}

// 解锁
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->type) {
        case PPDB_SYNC_MUTEX:
            __atomic_store_n(&sync->mutex, 0, __ATOMIC_SEQ_CST);
            break;
        case PPDB_SYNC_SPINLOCK:
            __atomic_store_n(&sync->spinlock, 0, __ATOMIC_SEQ_CST);
            break;
        case PPDB_SYNC_RWLOCK:
            __atomic_store_n(&sync->rwlock.writer, 0, __ATOMIC_SEQ_CST);
            break;
        default:
            return PPDB_ERR_INVALID_ARG;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    if (sync->type != PPDB_SYNC_RWLOCK) return PPDB_ERR_INVALID_ARG;
    
    // 等待写者完成
    while (__atomic_load_n(&sync->rwlock.writer, __ATOMIC_SEQ_CST) != 0) {
        usleep(1);
    }
    
    // 增加读者计数
    __atomic_add_fetch(&sync->rwlock.readers, 1, __ATOMIC_SEQ_CST);
    
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    if (sync->type != PPDB_SYNC_RWLOCK) return PPDB_ERR_INVALID_ARG;
    
    // 减少读者计数
    __atomic_sub_fetch(&sync->rwlock.readers, 1, __ATOMIC_SEQ_CST);
    
    return PPDB_OK;
}

// 文件同步函数
ppdb_error_t ppdb_sync_file(const char* filename) {
    if (!filename) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 使用 Cosmopolitan 的 POSIX 兼容层
    int fd = open(filename, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        PPDB_LOG_ERROR("Failed to open file for sync: %s (errno: %d)", filename, errno);
        return PPDB_ERR_IO;
    }

    // fsync 在 Cosmopolitan 中是跨平台的
    int ret = fsync(fd);
    close(fd);

    if (ret != 0) {
        PPDB_LOG_ERROR("Failed to sync file: %s (errno: %d)", filename, errno);
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

// 文件描述符同步函数
ppdb_error_t ppdb_sync_fd(int fd) {
    if (fd < 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    // fsync 在 Cosmopolitan 中是跨平台的
    int ret = fsync(fd);
    if (ret != 0) {
        PPDB_LOG_ERROR("Failed to sync file descriptor: %d (errno: %d)", fd, errno);
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

// 创建同步原语对象
ppdb_sync_t* ppdb_sync_create(void) {
    ppdb_sync_t* sync = malloc(sizeof(ppdb_sync_t));
    if (!sync) return NULL;

    // 初始化为默认的互斥锁模式
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000,
        .timeout_ms = 0
    };

    if (ppdb_sync_init(sync, &config) != PPDB_OK) {
        free(sync);
        return NULL;
    }

    return sync;
}
