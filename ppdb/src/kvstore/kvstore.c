#include <cosmopolitan.h>
#include "ppdb/kvstore.h"
#include "ppdb/memtable.h"
#include "ppdb/wal.h"
#include "ppdb/error.h"
#include "ppdb/logger.h"
#include "ppdb/fs.h"

// KVStore结构
struct ppdb_kvstore_t {
    char db_path[MAX_PATH_LENGTH];  // 数据库路径
    struct ppdb_memtable_t* table;  // 内存表
    struct ppdb_wal_t* wal;         // WAL日志
    pthread_mutex_t mutex;          // 并发控制（仅在有锁模式下使用）
    ppdb_mode_t mode;              // 运行模式
};

// 创建KVStore
ppdb_error_t ppdb_kvstore_create(const ppdb_kvstore_config_t* config, ppdb_kvstore_t** store) {
    if (!config || !store) {
        ppdb_log_error("Invalid arguments: config=%p, store=%p", config, store);
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Creating KVStore at: %s (mode: %s)", 
                  config->dir_path,
                  config->mode == PPDB_MODE_LOCKFREE ? "lock-free" : "locked");

    // 分配KVStore结构
    ppdb_kvstore_t* new_store = (ppdb_kvstore_t*)malloc(sizeof(ppdb_kvstore_t));
    if (!new_store) {
        ppdb_log_error("Failed to allocate KVStore");
        return PPDB_ERR_NO_MEMORY;
    }

    // 初始化结构
    memset(new_store, 0, sizeof(ppdb_kvstore_t));
    new_store->mode = config->mode;

    // 复制路径
    size_t path_len = strlen(config->dir_path);
    if (path_len >= MAX_PATH_LENGTH) {
        return PPDB_ERR_PATH_TOO_LONG;
    }
    memcpy(new_store->db_path, config->dir_path, path_len);
    new_store->db_path[path_len] = '\0';

    // 创建MemTable（根据模式选择实现）
    ppdb_error_t err;
    if (config->mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_memtable_create_lockfree(config->memtable_size, &new_store->table);
    } else {
        err = ppdb_memtable_create(config->memtable_size, &new_store->table);
    }
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create MemTable: %d", err);
        free(new_store);
        return err;
    }

    // 初始化互斥锁（仅在有锁模式下）
    if (config->mode == PPDB_MODE_LOCKED) {
        if (pthread_mutex_init(&new_store->mutex, NULL) != 0) {
            ppdb_log_error("Failed to initialize mutex");
            ppdb_memtable_destroy(new_store->table);
            free(new_store);
            return PPDB_ERR_MUTEX_ERROR;
        }
    }

    // 构造WAL路径
    char wal_path[MAX_PATH_LENGTH];
    int written = snprintf(wal_path, sizeof(wal_path), "%s.wal", config->dir_path);
    if (written < 0 || written >= (int)sizeof(wal_path)) {
        if (config->mode == PPDB_MODE_LOCKED) {
            pthread_mutex_destroy(&new_store->mutex);
        }
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return PPDB_ERR_PATH_TOO_LONG;
    }

    // 创建WAL配置
    ppdb_wal_config_t wal_config = {
        .segment_size = config->l0_size,
        .sync_write = true,
        .mode = config->mode  // 传递运行模式
    };
    memcpy(wal_config.dir_path, wal_path, sizeof(wal_path));

    // 创建WAL（根据模式选择实现）
    if (config->mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_wal_create_lockfree(&wal_config, &new_store->wal);
    } else {
        err = ppdb_wal_create(&wal_config, &new_store->wal);
    }
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create WAL: %d", err);
        if (config->mode == PPDB_MODE_LOCKED) {
            pthread_mutex_destroy(&new_store->mutex);
        }
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return err;
    }

    // 从WAL恢复数据
    if (config->mode == PPDB_MODE_LOCKED) {
        pthread_mutex_lock(&new_store->mutex);
    }
    err = ppdb_wal_recover(new_store->wal, &new_store->table);
    if (config->mode == PPDB_MODE_LOCKED) {
        pthread_mutex_unlock(&new_store->mutex);
    }
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to recover from WAL: %d", err);
        if (config->mode == PPDB_MODE_LOCKED) {
            pthread_mutex_destroy(&new_store->mutex);
        }
        ppdb_wal_destroy(new_store->wal);
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return err;
    }

    *store = new_store;
    ppdb_log_info("Successfully created KVStore at: %s", config->dir_path);
    return PPDB_OK;
}

// 关闭KVStore
void ppdb_kvstore_close(ppdb_kvstore_t* store) {
    if (!store) return;

    ppdb_log_info("Closing KVStore at: %s", store->db_path);

    if (store->mode == PPDB_MODE_LOCKED) {
        pthread_mutex_lock(&store->mutex);
    }

    // 先关闭WAL，确保所有数据都写入磁盘
    if (store->wal) {
        if (store->mode == PPDB_MODE_LOCKFREE) {
            ppdb_wal_close_lockfree(store->wal);
        } else {
            ppdb_wal_close(store->wal);
        }
        store->wal = NULL;
    }

    // 再销毁MemTable
    if (store->table) {
        if (store->mode == PPDB_MODE_LOCKFREE) {
            ppdb_memtable_destroy_lockfree(store->table);
        } else {
            ppdb_memtable_destroy(store->table);
        }
        store->table = NULL;
    }

    if (store->mode == PPDB_MODE_LOCKED) {
        pthread_mutex_unlock(&store->mutex);
        pthread_mutex_destroy(&store->mutex);
    }
    free(store);
}

// 销毁KVStore及其所有数据
void ppdb_kvstore_destroy(ppdb_kvstore_t* store) {
    if (!store) return;

    ppdb_log_info("Destroying KVStore at: %s", store->db_path);

    if (store->mode == PPDB_MODE_LOCKED) {
        pthread_mutex_lock(&store->mutex);
    }

    // 先销毁WAL
    if (store->wal) {
        if (store->mode == PPDB_MODE_LOCKFREE) {
            ppdb_wal_destroy_lockfree(store->wal);
        } else {
            ppdb_wal_destroy(store->wal);
        }
        store->wal = NULL;
    }

    // 再销毁MemTable
    if (store->table) {
        if (store->mode == PPDB_MODE_LOCKFREE) {
            ppdb_memtable_destroy_lockfree(store->table);
        } else {
            ppdb_memtable_destroy(store->table);
        }
        store->table = NULL;
    }

    if (store->mode == PPDB_MODE_LOCKED) {
        pthread_mutex_unlock(&store->mutex);
        pthread_mutex_destroy(&store->mutex);
    }
    free(store);
}

// 写入键值对
ppdb_error_t ppdb_kvstore_put(ppdb_kvstore_t* store,
                             const uint8_t* key, size_t key_len,
                             const uint8_t* value, size_t value_len) {
    if (!store || !key || !value || key_len == 0 || value_len == 0) {
        return PPDB_ERR_NULL_POINTER;
    }

    ppdb_error_t err;
    if (store->mode == PPDB_MODE_LOCKFREE) {
        // 无锁模式：直接写WAL和MemTable
        err = ppdb_wal_write_lockfree(store->wal, PPDB_WAL_RECORD_PUT,
                                    key, key_len, value, value_len);
        if (err != PPDB_OK) {
            return err;
        }

        err = ppdb_memtable_put_lockfree(store->table, key, key_len, value, value_len);
        if (err == PPDB_ERR_FULL) {
            // 如果MemTable已满，创建新的MemTable
            ppdb_memtable_t* new_table = NULL;
            size_t size_limit = ppdb_memtable_max_size_lockfree(store->table);
            err = ppdb_memtable_create_lockfree(size_limit, &new_table);
            if (err != PPDB_OK) {
                return err;
            }

            // 将旧MemTable持久化（这里简化处理，实际应该写入SSTable）
            ppdb_memtable_destroy_lockfree(store->table);
            store->table = new_table;

            // 重试写入
            err = ppdb_memtable_put_lockfree(store->table, key, key_len, value, value_len);
        }
    } else {
        // 有锁模式：使用互斥锁保护
        pthread_mutex_lock(&store->mutex);

        // 先写WAL
        err = ppdb_wal_write(store->wal, PPDB_WAL_RECORD_PUT,
                           key, key_len, value, value_len);
        if (err != PPDB_OK) {
            pthread_mutex_unlock(&store->mutex);
            return err;
        }

        // 写入MemTable
        err = ppdb_memtable_put(store->table, key, key_len, value, value_len);
        if (err == PPDB_ERR_FULL) {
            // 如果MemTable已满，创建新的MemTable
            ppdb_memtable_t* new_table = NULL;
            size_t size_limit = ppdb_memtable_max_size(store->table);
            err = ppdb_memtable_create(size_limit, &new_table);
            if (err != PPDB_OK) {
                pthread_mutex_unlock(&store->mutex);
                return err;
            }

            // 将旧MemTable持久化（这里简化处理，实际应该写入SSTable）
            ppdb_memtable_destroy(store->table);
            store->table = new_table;

            // 重试写入
            err = ppdb_memtable_put(store->table, key, key_len, value, value_len);
        }

        pthread_mutex_unlock(&store->mutex);
    }

    return err;
}

// 读取键值对
ppdb_error_t ppdb_kvstore_get(ppdb_kvstore_t* store,
                             const uint8_t* key, size_t key_len,
                             uint8_t* value, size_t* value_len) {
    if (!store || !key || !value || !value_len || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err;
    if (store->mode == PPDB_MODE_LOCKFREE) {
        // 无锁模式：直接从MemTable读取
        err = ppdb_memtable_get_lockfree(store->table, key, key_len, value, value_len);
    } else {
        // 有锁模式：使用互斥锁保护
        pthread_mutex_lock(&store->mutex);
        err = ppdb_memtable_get(store->table, key, key_len, value, value_len);
        pthread_mutex_unlock(&store->mutex);
    }

    return err;
}

// 删除键值对
ppdb_error_t ppdb_kvstore_delete(ppdb_kvstore_t* store,
                                const uint8_t* key, size_t key_len) {
    if (!store || !key || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err;
    if (store->mode == PPDB_MODE_LOCKFREE) {
        // 无锁模式：直接写WAL和MemTable
        err = ppdb_wal_write_lockfree(store->wal, PPDB_WAL_RECORD_DELETE,
                                    key, key_len, NULL, 0);
        if (err != PPDB_OK) {
            return err;
        }

        err = ppdb_memtable_delete_lockfree(store->table, key, key_len);
    } else {
        // 有锁模式：使用互斥锁保护
        pthread_mutex_lock(&store->mutex);

        // 先写WAL
        err = ppdb_wal_write(store->wal, PPDB_WAL_RECORD_DELETE,
                           key, key_len, NULL, 0);
        if (err != PPDB_OK) {
            pthread_mutex_unlock(&store->mutex);
            return err;
        }

        // 从MemTable删除
        err = ppdb_memtable_delete(store->table, key, key_len);

        pthread_mutex_unlock(&store->mutex);
    }

    return err;
}