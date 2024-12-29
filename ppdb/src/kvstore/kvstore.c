#include <cosmopolitan.h>
#include "ppdb/kvstore.h"
#include "ppdb/memtable.h"
#include "ppdb/wal.h"
#include "ppdb/error.h"
#include "ppdb/logger.h"
#include "ppdb/fs.h"

struct ppdb_kvstore_t {
    char db_path[MAX_PATH_LENGTH];
    struct ppdb_memtable_t* table;
    struct ppdb_wal_t* wal;
    pthread_mutex_t mutex;
    ppdb_mode_t mode;
};

static inline void lock_if_needed(ppdb_kvstore_t* store) {
    if (store->mode == PPDB_MODE_LOCKED) pthread_mutex_lock(&store->mutex);
}

static inline void unlock_if_needed(ppdb_kvstore_t* store) {
    if (store->mode == PPDB_MODE_LOCKED) pthread_mutex_unlock(&store->mutex);
}

static ppdb_error_t create_memtable(ppdb_mode_t mode, size_t size, struct ppdb_memtable_t** table) {
    if (!table) {
        ppdb_log_error("Invalid argument: table is NULL");
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err;
    if (mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_memtable_create_lockfree(size, table);
    } else {
        err = ppdb_memtable_create(size, table);
    }

    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create MemTable: %s", ppdb_error_string(err));
    }
    return err;
}

static ppdb_error_t create_wal(ppdb_mode_t mode, ppdb_wal_config_t* config, struct ppdb_wal_t** wal) {
    if (!config || !wal) {
        ppdb_log_error("Invalid arguments: config=%p, wal=%p", config, wal);
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err;
    if (mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_wal_create_lockfree(config, wal);
    } else {
        err = ppdb_wal_create(config, wal);
    }

    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create WAL: %s", ppdb_error_string(err));
    }
    return err;
}

// Forward declaration
static void cleanup_store(ppdb_kvstore_t* store, bool destroy);

ppdb_error_t ppdb_kvstore_create(const ppdb_kvstore_config_t* config, ppdb_kvstore_t** store) {
    if (!config || !store || !config->dir_path || config->dir_path[0] == '\0') {
        ppdb_log_error("Invalid arguments: config=%p, store=%p", config, store);
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Creating KVStore at: %s (mode: %s)", 
                  config->dir_path,
                  config->mode == PPDB_MODE_LOCKFREE ? "lock-free" : "locked");

    // 分配KVStore结构
    ppdb_kvstore_t* new_store = (ppdb_kvstore_t*)calloc(1, sizeof(ppdb_kvstore_t));
    if (!new_store) {
        ppdb_log_error("Failed to allocate KVStore");
        return PPDB_ERR_NO_MEMORY;
    }

    // 初始化基本字段
    new_store->mode = config->mode;  // 先设置模式，这样cleanup_store才能正确工作
    new_store->table = NULL;
    new_store->wal = NULL;

    // 复制配置
    size_t db_path_len = strlen(config->dir_path);
    if (db_path_len >= sizeof(new_store->db_path)) {
        ppdb_log_error("Directory path too long");
        cleanup_store(new_store, true);
        return PPDB_ERR_PATH_TOO_LONG;
    }
    memcpy(new_store->db_path, config->dir_path, db_path_len);
    new_store->db_path[db_path_len] = '\0';

    // 初始化互斥锁
    if (config->mode == PPDB_MODE_LOCKED) {
        if (pthread_mutex_init(&new_store->mutex, NULL) != 0) {
            ppdb_log_error("Failed to initialize mutex");
            cleanup_store(new_store, true);
            return PPDB_ERR_MUTEX_ERROR;
        }
    }

    // 创建目录
    if (!ppdb_fs_dir_exists(config->dir_path)) {
        ppdb_error_t err = ppdb_ensure_directory(config->dir_path);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to create directory: %s", config->dir_path);
            cleanup_store(new_store, true);
            return err;
        }
    }

    // 等待目录创建完成
    usleep(100000);  // 100ms

    // 创建MemTable
    ppdb_error_t err = create_memtable(config->mode, config->memtable_size, &new_store->table);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create MemTable: %s", ppdb_error_string(err));
        cleanup_store(new_store, true);
        return err;
    }

    // 等待MemTable创建完成
    usleep(100000);  // 100ms

    // 创建WAL
    ppdb_wal_config_t wal_config = {
        .dir_path = {0},
        .segment_size = WAL_SEGMENT_SIZE,
        .sync_write = true,  // 默认同步写入
        .mode = config->mode
    };

    if (db_path_len > MAX_PATH_LENGTH - 5) {  // 5 = len("/wal") + 1
        ppdb_log_error("KVStore directory path too long");
        cleanup_store(new_store, true);
        return PPDB_ERR_PATH_TOO_LONG;
    }

    ssize_t wal_path_len = snprintf(wal_config.dir_path, sizeof(wal_config.dir_path), 
                                   "%s/wal", config->dir_path);
    if (wal_path_len < 0 || (size_t)wal_path_len >= sizeof(wal_config.dir_path)) {
        ppdb_log_error("Failed to construct WAL directory path");
        cleanup_store(new_store, true);
        return PPDB_ERR_PATH_TOO_LONG;
    }

    err = create_wal(config->mode, &wal_config, &new_store->wal);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create WAL: %s", ppdb_error_string(err));
        cleanup_store(new_store, true);
        return err;
    }

    // 等待WAL创建完成
    usleep(100000);  // 100ms

    // 从WAL恢复数据
    if (config->mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_wal_recover_lockfree(new_store->wal, &new_store->table);
    } else {
        err = ppdb_wal_recover(new_store->wal, &new_store->table);
    }

    if (err != PPDB_OK) {
        ppdb_log_error("Failed to recover from WAL: %s", ppdb_error_string(err));
        cleanup_store(new_store, true);
        return err;
    }

    // 等待WAL恢复完成
    usleep(100000);  // 100ms

    *store = new_store;
    ppdb_log_info("KVStore created successfully");
    return PPDB_OK;
}

static void cleanup_store(ppdb_kvstore_t* store, bool destroy) {
    if (!store) return;

    // 保存模式和指针
    ppdb_mode_t mode = store->mode;
    ppdb_wal_t* wal = store->wal;
    ppdb_memtable_t* table = store->table;

    // 清空指针防止重复释放
    store->wal = NULL;
    store->table = NULL;

    // 如果是有锁模式，先加锁
    if (mode == PPDB_MODE_LOCKED) {
        pthread_mutex_lock(&store->mutex);
    }

    // 清零敏感数据
    memset(store->db_path, 0, sizeof(store->db_path));

    // 先清理WAL
    if (wal) {
        if (destroy) {
            if (mode == PPDB_MODE_LOCKFREE) {
                ppdb_wal_destroy_lockfree(wal);
            } else {
                ppdb_wal_destroy(wal);
            }
        } else {
            if (mode == PPDB_MODE_LOCKFREE) {
                ppdb_wal_close_lockfree(wal);
            } else {
                ppdb_wal_close(wal);
            }
        }
        wal = NULL;
    }

    // 清理MemTable
    if (table) {
        if (destroy) {
            if (mode == PPDB_MODE_LOCKFREE) {
                ppdb_memtable_destroy_lockfree(table);
            } else {
                ppdb_memtable_destroy(table);
            }
        } else {
            if (mode == PPDB_MODE_LOCKFREE) {
                ppdb_memtable_close_lockfree(table);
            } else {
                ppdb_memtable_close(table);
            }
        }
        table = NULL;
    }

    // 如果是有锁模式，解锁并销毁互斥锁
    if (mode == PPDB_MODE_LOCKED) {
        pthread_mutex_unlock(&store->mutex);
        pthread_mutex_destroy(&store->mutex);
    }

    // 清零整个结构
    memset(store, 0, sizeof(ppdb_kvstore_t));

    // 最后释放结构体
    free(store);
}

// 关闭KVStore
void ppdb_kvstore_close(ppdb_kvstore_t* store) {
    if (!store) return;
    cleanup_store(store, false);
}

// 销毁KVStore及其所有数据
void ppdb_kvstore_destroy(ppdb_kvstore_t* store) {
    if (!store) return;
    cleanup_store(store, true);
}

static ppdb_error_t handle_memtable_full(ppdb_kvstore_t* store, 
                                        const uint8_t* key, size_t key_len,
                                        const uint8_t* value, size_t value_len) {
    ppdb_memtable_t* new_table = NULL;
    size_t size_limit = store->mode == PPDB_MODE_LOCKFREE ? 
                       ppdb_memtable_max_size_lockfree(store->table) : 
                       ppdb_memtable_max_size(store->table);

    // 创建新的MemTable
    ppdb_error_t err = create_memtable(store->mode, size_limit, &new_table);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create new MemTable: %s", ppdb_error_string(err));
        return err;
    }

    // 将旧的MemTable持久化（这里简化处理，实际应该写入SSTable）
    if (store->mode == PPDB_MODE_LOCKFREE) {
        ppdb_memtable_destroy_lockfree(store->table);
    } else {
        ppdb_memtable_destroy(store->table);
    }
    store->table = new_table;

    // 重试写入
    if (store->mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_memtable_put_lockfree(store->table, key, key_len, value, value_len);
    } else {
        err = ppdb_memtable_put(store->table, key, key_len, value, value_len);
    }

    if (err != PPDB_OK) {
        ppdb_log_error("Failed to put key-value pair after MemTable switch: %s", ppdb_error_string(err));
    }
    return err;
}

ppdb_error_t ppdb_kvstore_put(ppdb_kvstore_t* store,
                             const uint8_t* key, size_t key_len,
                             const uint8_t* value, size_t value_len) {
    if (!store || !key || !value || key_len == 0 || value_len == 0) {
        ppdb_log_error("Invalid arguments: store=%p, key=%p, key_len=%zu, value=%p, value_len=%zu",
                      store, key, key_len, value, value_len);
        return PPDB_ERR_INVALID_ARG;
    }

    lock_if_needed(store);

    // 先写WAL
    ppdb_error_t err;
    if (store->mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_wal_write_lockfree(store->wal, PPDB_WAL_RECORD_PUT, key, key_len, value, value_len);
    } else {
        err = ppdb_wal_write(store->wal, PPDB_WAL_RECORD_PUT, key, key_len, value, value_len);
    }

    if (err != PPDB_OK) {
        ppdb_log_error("Failed to write WAL: %s", ppdb_error_string(err));
        unlock_if_needed(store);
        return err;
    }

    // 写入MemTable
    if (store->mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_memtable_put_lockfree(store->table, key, key_len, value, value_len);
    } else {
        err = ppdb_memtable_put(store->table, key, key_len, value, value_len);
    }

    if (err == PPDB_ERR_FULL) {
        // 处理MemTable已满的情况
        err = handle_memtable_full(store, key, key_len, value, value_len);
    }

    if (err != PPDB_OK) {
        ppdb_log_error("Failed to put key-value pair: %s", ppdb_error_string(err));
    }

    unlock_if_needed(store);
    return err;
}

ppdb_error_t ppdb_kvstore_get(ppdb_kvstore_t* store,
                             const uint8_t* key, size_t key_len,
                             uint8_t** value, size_t* value_len) {
    if (!store || !key || !value_len || key_len == 0) {
        ppdb_log_error("Invalid arguments: store=%p, key=%p, key_len=%zu, value_len=%p",
                      store, key, key_len, value_len);
        return PPDB_ERR_INVALID_ARG;
    }

    lock_if_needed(store);

    // 从MemTable中读取
    ppdb_error_t err;
    if (store->mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_memtable_get_lockfree(store->table, key, key_len, value, value_len);
    } else {
        err = ppdb_memtable_get(store->table, key, key_len, value, value_len);
    }

    if (err != PPDB_OK && err != PPDB_ERR_NOT_FOUND) {
        ppdb_log_error("Failed to get key-value pair: %s", ppdb_error_string(err));
    }

    unlock_if_needed(store);
    return err;
}

ppdb_error_t ppdb_kvstore_delete(ppdb_kvstore_t* store,
                                const uint8_t* key, size_t key_len) {
    if (!store || !key || key_len == 0) {
        ppdb_log_error("Invalid arguments: store=%p, key=%p, key_len=%zu",
                      store, key, key_len);
        return PPDB_ERR_INVALID_ARG;
    }

    lock_if_needed(store);

    // 先写WAL
    ppdb_error_t err;
    if (store->mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_wal_write_lockfree(store->wal, PPDB_WAL_RECORD_DELETE, key, key_len, NULL, 0);
    } else {
        err = ppdb_wal_write(store->wal, PPDB_WAL_RECORD_DELETE, key, key_len, NULL, 0);
    }

    if (err != PPDB_OK) {
        ppdb_log_error("Failed to write WAL: %s", ppdb_error_string(err));
        unlock_if_needed(store);
        return err;
    }

    // 从MemTable中删除
    if (store->mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_memtable_delete_lockfree(store->table, key, key_len);
    } else {
        err = ppdb_memtable_delete(store->table, key, key_len);
    }

    if (err != PPDB_OK && err != PPDB_ERR_NOT_FOUND) {
        ppdb_log_error("Failed to delete key-value pair: %s", ppdb_error_string(err));
    }

    unlock_if_needed(store);
    return err;
}