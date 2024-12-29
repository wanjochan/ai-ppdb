#include <cosmopolitan.h>
#include "ppdb/kvstore.h"
#include "ppdb/memtable.h"
#include "ppdb/wal.h"
#include "ppdb/monitor.h"
#include "internal/kvstore_internal.h"
#include "internal/kvstore_fs.h"
#include "internal/kvstore_logger.h"

// 创建内存表
static ppdb_error_t create_memtable(size_t size, ppdb_memtable_t** table, bool use_sharding) {
    if (!table) return PPDB_ERR_NULL_POINTER;

    ppdb_error_t err;
    if (use_sharding) {
        err = ppdb_memtable_create_sharded(size, table);
    } else {
        err = ppdb_memtable_create(size, table);
    }

    return err;
}

// 创建WAL
static ppdb_error_t create_wal(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    if (!config || !wal) return PPDB_ERR_NULL_POINTER;
    return ppdb_wal_create(config, wal);
}

// 创建KVStore
ppdb_error_t ppdb_kvstore_create(const ppdb_kvstore_config_t* config, ppdb_kvstore_t** store) {
    if (!config || !store || !config->data_dir || config->data_dir[0] == '\0') {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Creating KVStore in %s (mode: %s)",
                  config->data_dir,
                  config->use_sharding ? "sharded" : "non-sharded");

    // 分配内存
    ppdb_kvstore_t* new_store = (ppdb_kvstore_t*)calloc(1, sizeof(ppdb_kvstore_t));
    if (!new_store) {
        return PPDB_ERR_NO_MEMORY;
    }

    // 初始化基本字段
    new_store->using_sharded = config->use_sharding;
    new_store->adaptive_enabled = config->adaptive_sharding;
    new_store->is_locked = false;

    // 复制数据目录路径
    size_t path_len = strlen(config->data_dir);
    if (path_len >= sizeof(new_store->db_path)) {
        free(new_store);
        return PPDB_ERR_PATH_TOO_LONG;
    }
    memcpy(new_store->db_path, config->data_dir, path_len);
    new_store->db_path[path_len] = '\0';

    // 初始化互斥锁
    mutex_init(&new_store->mutex);

    // 创建数据目录
    if (!ppdb_fs_dir_exists(config->data_dir)) {
        ppdb_error_t err = ppdb_ensure_directory(config->data_dir);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to create directory: %s", config->data_dir);
            cleanup_store(new_store);
            return err;
        }
    }

    // 创建内存表
    ppdb_error_t err = create_memtable(config->memtable_size, &new_store->table, config->use_sharding);
    if (err != PPDB_OK) {
        cleanup_store(new_store);
        return err;
    }

    // 创建WAL
    err = create_wal(&config->wal, &new_store->wal);
    if (err != PPDB_OK) {
        cleanup_store(new_store);
        return err;
    }

    // 从WAL恢复数据
    err = ppdb_wal_recover(new_store->wal, new_store->table);
    if (err != PPDB_OK) {
        cleanup_store(new_store);
        return err;
    }

    // 创建监控器
    ppdb_monitor_t* monitor;
    err = ppdb_monitor_create(&monitor);
    if (err != PPDB_OK) {
        cleanup_store(new_store);
        return err;
    }
    new_store->monitor = monitor;

    *store = new_store;
    return PPDB_OK;
}

// 清理KVStore
static void cleanup_store(ppdb_kvstore_t* store) {
    if (!store) return;

    if (store->wal) {
        ppdb_wal_destroy(store->wal);
        store->wal = NULL;
    }

    if (store->table) {
        ppdb_memtable_destroy(store->table);
        store->table = NULL;
    }

    if (store->monitor) {
        ppdb_monitor_destroy(store->monitor);
        store->monitor = NULL;
    }

    mutex_destroy(&store->mutex);
    free(store);
}

// 检查并切换内存表
static ppdb_error_t check_and_switch_memtable(ppdb_kvstore_t* store) {
    if (!store) return PPDB_ERR_NULL_POINTER;

    if (!store->using_sharded && ppdb_monitor_should_switch(store->monitor)) {
        ppdb_memtable_t* new_table;
        ppdb_error_t err = create_memtable(ppdb_memtable_max_size(store->table),
                                         &new_table,
                                         store->using_sharded);
        if (err != PPDB_OK) return err;

        // 迁移数据
        ppdb_memtable_iterator_t* it;
        err = ppdb_memtable_iterator_create(store->table, &it);
        if (err != PPDB_OK) {
            ppdb_memtable_destroy(new_table);
            return err;
        }

        void* key;
        size_t key_len;
        void* value;
        size_t value_len;

        while ((err = ppdb_memtable_iterator_next(it, &key, &key_len, &value, &value_len)) == PPDB_OK) {
            err = ppdb_memtable_put(new_table, key, key_len, value, value_len);
            if (err != PPDB_OK) {
                ppdb_memtable_iterator_destroy(it);
                ppdb_memtable_destroy(new_table);
                return err;
            }
        }

        ppdb_memtable_iterator_destroy(it);

        // 切换内存表
        ppdb_memtable_t* old_table = store->table;
        store->table = new_table;
        ppdb_memtable_destroy(old_table);
    }

    return PPDB_OK;
}

// 写入键值对
ppdb_error_t ppdb_kvstore_put(ppdb_kvstore_t* store,
                             const void* key, size_t key_len,
                             const void* value, size_t value_len) {
    if (!store || !key || !value) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0 || value_len == 0) return PPDB_ERR_INVALID_ARG;

    ppdb_error_t err;
    uint64_t start_time = now_us();

    // 开始监控
    ppdb_monitor_op_start(store->monitor);

    // 写入WAL
    if (store->using_sharded) {
        err = ppdb_wal_write_lockfree(store->wal, PPDB_WAL_RECORD_PUT, key, key_len, value, value_len);
    } else {
        mutex_lock_raw(&store->mutex);
        err = ppdb_wal_write(store->wal, PPDB_WAL_RECORD_PUT, key, key_len, value, value_len);
        mutex_unlock_raw(&store->mutex);
    }

    if (err != PPDB_OK) {
        ppdb_monitor_op_end(store->monitor, now_us() - start_time);
        return err;
    }

    // 写入内存表
    if (store->using_sharded) {
        err = ppdb_memtable_put_lockfree(store->table, key, key_len, value, value_len);
    } else {
        mutex_lock_raw(&store->mutex);
        err = ppdb_memtable_put(store->table, key, key_len, value, value_len);
        mutex_unlock_raw(&store->mutex);
    }

    if (err == PPDB_ERR_FULL) {
        err = handle_memtable_full(store, key, key_len, value, value_len);
    }

    uint64_t latency_us = now_us() - start_time;
    ppdb_monitor_op_end(store->monitor, latency_us);

    return err;
}

// 读取键值对
ppdb_error_t ppdb_kvstore_get(ppdb_kvstore_t* store,
                             const void* key, size_t key_len,
                             void** value, size_t* value_len) {
    if (!store || !key || !value || !value_len) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARG;

    ppdb_error_t err;
    uint64_t start_time = now_us();

    // 从内存表读取
    if (store->using_sharded) {
        err = ppdb_memtable_get_lockfree(store->table, key, key_len, value, value_len);
    } else {
        mutex_lock_raw(&store->mutex);
        err = ppdb_memtable_get(store->table, key, key_len, value, value_len);
        mutex_unlock_raw(&store->mutex);
    }

    uint64_t latency_us = now_us() - start_time;
    ppdb_monitor_op_end(store->monitor, latency_us);

    return err;
}

// 删除键值对
ppdb_error_t ppdb_kvstore_delete(ppdb_kvstore_t* store,
                                const void* key, size_t key_len) {
    if (!store || !key) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARG;

    ppdb_error_t err;
    uint64_t start_time = now_us();

    // 写入WAL
    if (store->using_sharded) {
        err = ppdb_wal_write_lockfree(store->wal, PPDB_WAL_RECORD_DELETE, key, key_len, NULL, 0);
    } else {
        mutex_lock_raw(&store->mutex);
        err = ppdb_wal_write(store->wal, PPDB_WAL_RECORD_DELETE, key, key_len, NULL, 0);
        mutex_unlock_raw(&store->mutex);
    }

    if (err != PPDB_OK) {
        ppdb_monitor_op_end(store->monitor, now_us() - start_time);
        return err;
    }

    // 从内存表删除
    if (store->using_sharded) {
        err = ppdb_memtable_delete_lockfree(store->table, key, key_len);
    } else {
        mutex_lock_raw(&store->mutex);
        err = ppdb_memtable_delete(store->table, key, key_len);
        mutex_unlock_raw(&store->mutex);
    }

    uint64_t latency_us = now_us() - start_time;
    ppdb_monitor_op_end(store->monitor, latency_us);

    return err;
}

// 关闭KVStore
void ppdb_kvstore_close(ppdb_kvstore_t* store) {
    if (!store) return;
    cleanup_store(store);
}

// 销毁KVStore
void ppdb_kvstore_destroy(ppdb_kvstore_t* store) {
    if (!store) return;
    cleanup_store(store);
}