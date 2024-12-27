#include <cosmopolitan.h>
#include "ppdb/kvstore.h"
#include "ppdb/memtable.h"
#include "ppdb/wal.h"
#include "ppdb/error.h"
#include "../common/logger.h"
#include "../common/fs.h"

// 打开KVStore
ppdb_error_t ppdb_kvstore_open(const char* path, ppdb_kvstore_t** store) {
    if (!path || !store) {
        ppdb_log_error("Invalid arguments: path=%p, store=%p", path, store);
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Opening KVStore at: %s", path);

    // 分配KVStore结构
    ppdb_kvstore_t* new_store = (ppdb_kvstore_t*)malloc(sizeof(ppdb_kvstore_t));
    if (!new_store) {
        ppdb_log_error("Failed to allocate KVStore");
        return PPDB_ERR_NO_MEMORY;
    }

    // 初始化结构
    memset(new_store, 0, sizeof(ppdb_kvstore_t));

    // 初始化路径
    strncpy(new_store->db_path, path, MAX_PATH_LENGTH - 1);
    new_store->db_path[MAX_PATH_LENGTH - 1] = '\0';

    // 创建MemTable
    ppdb_error_t err = ppdb_memtable_create(MEMTABLE_SIZE_LIMIT * 2, &new_store->table);  // 恢复时使用双倍大小限制
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create MemTable: %d", err);
        free(new_store);
        return err;
    }

    // 初始化互斥锁
    if (pthread_mutex_init(&new_store->mutex, NULL) != 0) {
        ppdb_log_error("Failed to initialize mutex");
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return PPDB_ERR_MUTEX_ERROR;
    }

    // 创建WAL目录
    char wal_path[MAX_PATH_LENGTH];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", path);
    err = ppdb_ensure_directory(wal_path);
    if (err != PPDB_OK && err != PPDB_ERR_EXISTS) {  // 忽略目录已存在的错误
        ppdb_log_error("Failed to create WAL directory: %s", wal_path);
        pthread_mutex_destroy(&new_store->mutex);
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return err;
    }

    // 创建WAL
    ppdb_wal_config_t wal_config = {0};  // 使用零初始化
    wal_config.segment_size = WAL_SEGMENT_SIZE;
    wal_config.sync_write = true;
    strncpy(wal_config.dir_path, wal_path, MAX_PATH_LENGTH - 1);
    wal_config.dir_path[MAX_PATH_LENGTH - 1] = '\0';

    err = ppdb_wal_create(&wal_config, &new_store->wal);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create WAL: %d", err);
        pthread_mutex_destroy(&new_store->mutex);
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return err;
    }

    // 从WAL恢复数据
    err = ppdb_wal_recover(new_store->wal, new_store->table);
    if (err != PPDB_OK && err != PPDB_ERR_FULL) {
        ppdb_log_error("Failed to recover from WAL: %d", err);
        ppdb_wal_destroy(new_store->wal);
        pthread_mutex_destroy(&new_store->mutex);
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return err;
    }

    // 重新创建MemTable,使用正常大小限制
    ppdb_memtable_t* new_table = NULL;
    err = ppdb_memtable_create(MEMTABLE_SIZE_LIMIT, &new_table);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create new MemTable: %d", err);
        ppdb_wal_destroy(new_store->wal);
        pthread_mutex_destroy(&new_store->mutex);
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return err;
    }

    // 将恢复的数据复制到新的MemTable
    err = ppdb_memtable_copy(new_store->table, new_table);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to copy data to new MemTable: %d", err);
        ppdb_memtable_destroy(new_table);
        ppdb_wal_destroy(new_store->wal);
        pthread_mutex_destroy(&new_store->mutex);
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return err;
    }

    // 销毁旧的MemTable
    ppdb_memtable_destroy(new_store->table);
    new_store->table = new_table;

    *store = new_store;
    ppdb_log_info("Successfully opened KVStore at: %s", path);
    return PPDB_OK;
}

// 关闭KVStore
void ppdb_kvstore_close(ppdb_kvstore_t* store) {
    if (!store) return;

    ppdb_log_info("Closing KVStore at: %s", store->db_path);

    if (store->wal) {
        ppdb_wal_close(store->wal);
        store->wal = NULL;
    }

    if (store->table) {
        ppdb_memtable_destroy(store->table);
        store->table = NULL;
    }

    pthread_mutex_destroy(&store->mutex);
    free(store);
}

// 写入键值对
ppdb_error_t ppdb_kvstore_put(ppdb_kvstore_t* store,
                             const uint8_t* key, size_t key_len,
                             const uint8_t* value, size_t value_len) {
    if (!store || !key || !value || key_len == 0 || value_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&store->mutex);

    // 写入WAL
    ppdb_error_t err = ppdb_wal_write(store->wal, PPDB_WAL_RECORD_PUT,
                                     key, key_len, value, value_len);
    if (err != PPDB_OK) {
        pthread_mutex_unlock(&store->mutex);
        return err;
    }

    // 写入MemTable
    err = ppdb_memtable_put(store->table, key, key_len, value, value_len);
    if (err != PPDB_OK) {
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
    if (!store || !key || !value || !value_len || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&store->mutex);

    // 从MemTable读取
    ppdb_error_t err = ppdb_memtable_get(store->table, key, key_len,
                                        value, value_len);

    pthread_mutex_unlock(&store->mutex);
    return err;
}

// 删除键值对
ppdb_error_t ppdb_kvstore_delete(ppdb_kvstore_t* store,
                                const uint8_t* key, size_t key_len) {
    if (!store || !key || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&store->mutex);

    // 写入WAL
    ppdb_error_t err = ppdb_wal_write(store->wal, PPDB_WAL_RECORD_DELETE,
                                     key, key_len, NULL, 0);
    if (err != PPDB_OK) {
        pthread_mutex_unlock(&store->mutex);
        return err;
    }

    // 从MemTable删除
    err = ppdb_memtable_delete(store->table, key, key_len);
    if (err != PPDB_OK) {
        pthread_mutex_unlock(&store->mutex);
        return err;
    }

    pthread_mutex_unlock(&store->mutex);
    return PPDB_OK;
}