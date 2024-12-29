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
    ppdb_monitor_t* monitor;        // 添加性能监控器
    bool using_sharded;             // 是否使用分片模式
    bool adaptive_enabled;          // 是否启用自适应分片
};

static inline void lock_if_needed(ppdb_kvstore_t* store) {
    if (store->mode == PPDB_MODE_LOCKED) pthread_mutex_lock(&store->mutex);
}

static inline void unlock_if_needed(ppdb_kvstore_t* store) {
    if (store->mode == PPDB_MODE_LOCKED) pthread_mutex_unlock(&store->mutex);
}

static ppdb_error_t create_memtable(ppdb_mode_t mode, size_t size, struct ppdb_memtable_t** table, bool use_sharded) {
    if (!table) {
        ppdb_log_error("Invalid argument: table is NULL");
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err;
    if (use_sharded) {
        // 使用分片模式
        err = ppdb_memtable_create_sharded(size, table);
    } else if (mode == PPDB_MODE_LOCKFREE) {
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

    // Allocate KVStore structure
    ppdb_kvstore_t* new_store = (ppdb_kvstore_t*)calloc(1, sizeof(ppdb_kvstore_t));
    if (!new_store) {
        ppdb_log_error("Failed to allocate KVStore");
        return PPDB_ERR_NO_MEMORY;
    }

    // Initialize basic fields
    new_store->mode = config->mode;  // Set mode first so cleanup_store works correctly
    new_store->table = NULL;
    new_store->wal = NULL;

    // Copy configuration
    size_t db_path_len = strlen(config->dir_path);
    if (db_path_len >= sizeof(new_store->db_path)) {
        ppdb_log_error("Directory path too long");
        cleanup_store(new_store, true);
        return PPDB_ERR_PATH_TOO_LONG;
    }
    memcpy(new_store->db_path, config->dir_path, db_path_len);
    new_store->db_path[db_path_len] = '\0';

    // Initialize mutex
    if (config->mode == PPDB_MODE_LOCKED) {
        if (pthread_mutex_init(&new_store->mutex, NULL) != 0) {
            ppdb_log_error("Failed to initialize mutex");
            cleanup_store(new_store, true);
            return PPDB_ERR_MUTEX_ERROR;
        }
    }

    // Create directory
    if (!ppdb_fs_dir_exists(config->dir_path)) {
        ppdb_error_t err = ppdb_ensure_directory(config->dir_path);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to create directory: %s", config->dir_path);
            cleanup_store(new_store, true);
            return err;
        }
    }

    // Wait for directory creation to complete
    usleep(100000);  // 100ms

    // Create MemTable
    ppdb_error_t err = create_memtable(config->mode, config->memtable_size, &new_store->table, false);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create MemTable: %s", ppdb_error_string(err));
        cleanup_store(new_store, true);
        return err;
    }

    // Wait for MemTable creation to complete
    usleep(100000);  // 100ms

    // Create WAL
    ppdb_wal_config_t wal_config = {
        .dir_path = {0},
        .segment_size = WAL_SEGMENT_SIZE,
        .sync_write = true,  // Default to synchronous write
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

    // Wait for WAL creation to complete
    usleep(100000);  // 100ms

    // Recover data from WAL
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

    // Wait for WAL recovery to complete
    usleep(100000);  // 100ms

    // 创建性能监控器，根据配置决定是否启用自适应分片
    new_store->monitor = ppdb_monitor_create();
    if (!new_store->monitor) {
        ppdb_log_error("Failed to create performance monitor");
        cleanup_store(new_store, true);
        return PPDB_ERR_NO_MEMORY;
    }
    
    // 设置自适应分片状态
    new_store->using_sharded = false;
    new_store->adaptive_enabled = config->adaptive_sharding;

    *store = new_store;
    ppdb_log_info("KVStore created successfully");
    return PPDB_OK;
}

static void cleanup_store(ppdb_kvstore_t* store, bool destroy) {
    if (!store) return;

    // Save mode and pointers
    ppdb_mode_t mode = store->mode;
    ppdb_wal_t* wal = store->wal;
    ppdb_memtable_t* table = store->table;

    // Clear pointers to prevent double-free
    store->wal = NULL;
    store->table = NULL;

    // If locked mode, lock first
    if (mode == PPDB_MODE_LOCKED) {
        pthread_mutex_lock(&store->mutex);
    }

    // Zero out sensitive data
    memset(store->db_path, 0, sizeof(store->db_path));

    // Clean up WAL
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
        usleep(500000);  // Wait 500ms to ensure WAL resources are released
    }

    // Clean up MemTable
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
        usleep(500000);  // Wait 500ms to ensure MemTable resources are released
    }

    // Clean up performance monitor
    if (store->monitor) {
        ppdb_monitor_destroy(store->monitor);
        store->monitor = NULL;
    }

    // If locked mode, unlock and destroy mutex
    if (mode == PPDB_MODE_LOCKED) {
        pthread_mutex_unlock(&store->mutex);
        pthread_mutex_destroy(&store->mutex);
        usleep(500000);  // Wait 500ms to ensure mutex resources are released
    }

    // Zero out entire structure
    memset(store, 0, sizeof(ppdb_kvstore_t));

    // Finally, free the structure
    free(store);
    usleep(500000);  // Wait 500ms to ensure memory is released
}

static ppdb_error_t check_and_switch_memtable(ppdb_kvstore_t* store) {
    if (!store || !store->monitor) return PPDB_ERR_INVALID_ARG;
    
    // 只在启用自适应分片时进行检查
    if (!store->adaptive_enabled) return PPDB_OK;

    // 检查是否需要切换到分片模式
    if (!store->using_sharded && ppdb_monitor_should_switch(store->monitor)) {
        ppdb_log_info("Switching to sharded memtable mode due to high load");
        
        // 创建新的分片memtable
        struct ppdb_memtable_t* new_table = NULL;
        ppdb_error_t err = create_memtable(store->mode, 
                                         ppdb_memtable_max_size(store->table),
                                         &new_table, 
                                         true);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to create sharded memtable: %s", ppdb_error_string(err));
            return err;
        }

        // 获取旧表的迭代器
        ppdb_memtable_iterator_t* it = ppdb_memtable_iterator_create(store->table);
        if (!it) {
            ppdb_memtable_destroy(new_table);
            return PPDB_ERR_NO_MEMORY;
        }

        // 迁移数据
        const void* key;
        size_t key_len;
        const void* value;
        size_t value_len;
        while (ppdb_memtable_iterator_next(it, &key, &key_len, &value, &value_len)) {
            err = ppdb_memtable_put(new_table, key, key_len, value, value_len);
            if (err != PPDB_OK) {
                ppdb_memtable_iterator_destroy(it);
                ppdb_memtable_destroy(new_table);
                return err;
            }
        }

        // 清理
        ppdb_memtable_iterator_destroy(it);
        ppdb_memtable_destroy(store->table);
        store->table = new_table;
        store->using_sharded = true;

        ppdb_log_info("Successfully switched to sharded memtable mode");
    }

    return PPDB_OK;
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

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    ppdb_monitor_op_start(store->monitor);
    
    // 检查是否需要切换到分片模式
    ppdb_error_t err = check_and_switch_memtable(store);
    if (err != PPDB_OK) return err;

    // Write to WAL
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

    // Write to MemTable
    if (store->mode == PPDB_MODE_LOCKFREE) {
        err = ppdb_memtable_put_lockfree(store->table, key, key_len, value, value_len);
    } else {
        err = ppdb_memtable_put(store->table, key, key_len, value, value_len);
    }

    if (err == PPDB_ERR_FULL) {
        // Handle MemTable full case
        err = handle_memtable_full(store, key, key_len, value, value_len);
    }

    if (err != PPDB_OK) {
        ppdb_log_error("Failed to put key-value pair: %s", ppdb_error_string(err));
    }

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    uint64_t latency_us = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
                         (end_time.tv_nsec - start_time.tv_nsec) / 1000;
    
    ppdb_monitor_op_end(store->monitor, latency_us);
    
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

    // Read from MemTable
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

    // Write to WAL
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

    // Delete from MemTable
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

// Close KVStore
void ppdb_kvstore_close(ppdb_kvstore_t* store) {
    if (store) cleanup_store(store, false);
}

// Destroy KVStore and all its data
void ppdb_kvstore_destroy(ppdb_kvstore_t* store) {
    if (store) cleanup_store(store, true);
}