#include <cosmopolitan.h>
#include "ppdb/kvstore.h"
#include "ppdb/memtable.h"
#include "ppdb/wal.h"
#include "ppdb/error.h"
#include "ppdb/defs.h"
#include "../common/logger.h"
#include "../common/fs.h"

// KVStore结构
struct ppdb_kvstore_t {
    char* path;                  // 数据库路径
    ppdb_memtable_t* memtable;  // 内存表
    ppdb_wal_t* wal;            // WAL日志
    pthread_mutex_t mutex;       // 并发控制
};

// 创建KVStore实例
ppdb_error_t ppdb_kvstore_open(const char* path, ppdb_kvstore_t** store) {
    if (!path || !store || path[0] == '\0') {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Opening KVStore at: %s", path);

    // 确保数据库目录存在
    char db_dir[MAX_PATH_LENGTH];
    strncpy(db_dir, path, sizeof(db_dir) - 1);
    db_dir[sizeof(db_dir) - 1] = '\0';

    ppdb_error_t err = PPDB_OK;
    char* last_sep = strrchr(db_dir, '/');
    if (last_sep) {
        *last_sep = '\0';
        err = ppdb_ensure_directory(db_dir);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to ensure database directory exists: %s", db_dir);
            return err;
        }
    }

    // 分配KVStore结构
    ppdb_kvstore_t* new_store = (ppdb_kvstore_t*)malloc(sizeof(ppdb_kvstore_t));
    if (!new_store) {
        ppdb_log_error("Failed to allocate memory for KVStore");
        return PPDB_ERR_NO_MEMORY;
    }

    // 初始化路径
    size_t path_len = strlen(path);
    new_store->path = (char*)malloc(path_len + 1);
    if (!new_store->path) {
        ppdb_log_error("Failed to allocate memory for path");
        free(new_store);
        return PPDB_ERR_NO_MEMORY;
    }
    strncpy(new_store->path, path, path_len + 1);

    // 创建MemTable
    err = ppdb_memtable_create(1024 * 1024, &new_store->memtable);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create MemTable");
        free(new_store->path);
        free(new_store);
        return err;
    }

    // 创建WAL
    char wal_path[MAX_PATH_LENGTH];
    int written = snprintf(wal_path, sizeof(wal_path), "%s.wal", path);
    if (written < 0 || written >= sizeof(wal_path)) {
        ppdb_log_error("Failed to format WAL path: path too long");
        ppdb_memtable_destroy(new_store->memtable);
        free(new_store->path);
        free(new_store);
        return PPDB_ERR_IO;
    }

    // 确保WAL目录存在
    char wal_dir[MAX_PATH_LENGTH];
    strncpy(wal_dir, wal_path, sizeof(wal_dir) - 1);
    wal_dir[sizeof(wal_dir) - 1] = '\0';
    char* wal_last_sep = strrchr(wal_dir, '/');
    if (wal_last_sep) {
        *wal_last_sep = '\0';
        err = ppdb_ensure_directory(wal_dir);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to ensure WAL directory exists: %s", wal_dir);
            ppdb_memtable_destroy(new_store->memtable);
            free(new_store->path);
            free(new_store);
            return err;
        }
    }

    ppdb_wal_config_t wal_config = {
        .dir_path = wal_path,
        .segment_size = 4096,
        .sync_write = true
    };
    err = ppdb_wal_create(&wal_config, &new_store->wal);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create WAL");
        ppdb_memtable_destroy(new_store->memtable);
        free(new_store->path);
        free(new_store);
        return err;
    }

    // 初始化互斥锁
    if (pthread_mutex_init(&new_store->mutex, NULL) != 0) {
        ppdb_log_error("Failed to initialize mutex");
        ppdb_wal_destroy(new_store->wal);
        ppdb_memtable_destroy(new_store->memtable);
        free(new_store->path);
        free(new_store);
        return PPDB_ERR_MUTEX_ERROR;
    }

    // 从WAL恢复数据
    err = ppdb_wal_recover(new_store->wal, new_store->memtable);
    if (err != PPDB_OK && err != PPDB_ERR_NOT_FOUND) {  // 允许WAL不存在的情况
        ppdb_log_error("Failed to recover from WAL");
        pthread_mutex_destroy(&new_store->mutex);
        ppdb_wal_destroy(new_store->wal);
        ppdb_memtable_destroy(new_store->memtable);
        free(new_store->path);
        free(new_store);
        return err;
    }

    ppdb_log_info("Successfully opened KVStore at: %s", path);
    *store = new_store;
    return PPDB_OK;
}

// 关闭KVStore实例
void ppdb_kvstore_close(ppdb_kvstore_t* store) {
    if (!store) return;

    ppdb_log_info("Closing KVStore at: %s", store->path);
    pthread_mutex_destroy(&store->mutex);
    ppdb_wal_destroy(store->wal);
    ppdb_memtable_destroy(store->memtable);
    free(store->path);
    free(store);
}

// 写入记录
ppdb_error_t ppdb_kvstore_put(ppdb_kvstore_t* store,
                             const uint8_t* key,
                             size_t key_len,
                             const uint8_t* value,
                             size_t value_len) {
    if (!store || !key || !value || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&store->mutex);

    // 写入WAL
    ppdb_error_t err = ppdb_wal_write(store->wal, PPDB_WAL_RECORD_PUT,
                                     key, key_len, value, value_len);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to write to WAL");
        pthread_mutex_unlock(&store->mutex);
        return err;
    }

    // 写入MemTable
    err = ppdb_memtable_put(store->memtable, key, key_len, value, value_len);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to write to MemTable");
        pthread_mutex_unlock(&store->mutex);
        return err;
    }

    pthread_mutex_unlock(&store->mutex);
    return PPDB_OK;
}

// 读取键值对
ppdb_error_t ppdb_kvstore_get(ppdb_kvstore_t* store,
                             const uint8_t* key, size_t key_len,
                             uint8_t* value, size_t* value_len) {
    if (!store || !key || !value || !value_len) {
        return PPDB_ERR_NULL_POINTER;
    }

    pthread_mutex_lock(&store->mutex);
    ppdb_error_t err = ppdb_memtable_get(store->memtable, key, key_len,
                                        value, value_len);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to read from MemTable");
    }
    pthread_mutex_unlock(&store->mutex);
    return err;
}

// 删除记录
ppdb_error_t ppdb_kvstore_delete(ppdb_kvstore_t* store,
                                const uint8_t* key,
                                size_t key_len) {
    if (!store || !key || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&store->mutex);

    // 写入WAL
    ppdb_error_t err = ppdb_wal_write(store->wal, PPDB_WAL_RECORD_DELETE,
                                     key, key_len, NULL, 0);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to write to WAL");
        pthread_mutex_unlock(&store->mutex);
        return err;
    }

    // 从MemTable中删除
    err = ppdb_memtable_delete(store->memtable, key, key_len);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to delete from MemTable");
        pthread_mutex_unlock(&store->mutex);
        return err;
    }

    pthread_mutex_unlock(&store->mutex);
    return PPDB_OK;
}