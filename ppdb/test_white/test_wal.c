#include <cosmopolitan.h>
#include "../include/ppdb/wal.h"
#include "../include/ppdb/memtable.h"
#include "../include/ppdb/defs.h"
#include "test_framework.h"

// 测试函数声明
static int test_wal_basic(void);
static int test_wal_concurrent(void);

// 测试用例定义
static const test_case_t wal_test_cases[] = {
    {"basic", test_wal_basic},
    {"concurrent", test_wal_concurrent}
};

// 测试套件定义
const test_suite_t wal_suite = {
    .name = "WAL",
    .cases = wal_test_cases,
    .num_cases = sizeof(wal_test_cases) / sizeof(wal_test_cases[0])
};

// 清理目录
static void cleanup_dir(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[MAX_PATH_LENGTH];
        snprintf(path, sizeof(path), "%s\\%s", dir_path, entry->d_name);
        unlink(path);  // 删除文件
    }

    closedir(dir);
    rmdir(dir_path);  // 删除目录
}

// 创建目录（如果不存在）
static ppdb_error_t ensure_directory(const char* path) {
    char dir_path[MAX_PATH_LENGTH];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    // 创建目录（如果不存在）
    if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
        ppdb_log_error("Failed to create directory: %s, error: %s", dir_path, strerror(errno));
        return PPDB_ERR_IO;
    }
    return PPDB_OK;
}

// 基本功能测试
ppdb_error_t test_wal_basic() {
    ppdb_log_info("Running basic WAL test...");

    char test_dir[MAX_PATH_LENGTH];
    if (!getcwd(test_dir, sizeof(test_dir))) {
        ppdb_log_error("Failed to get current working directory: %s", strerror(errno));
        return PPDB_ERR_IO;
    }
    ppdb_log_info("Current working directory: %s", test_dir);

    strcat(test_dir, "\\test_wal");
    ppdb_log_info("WAL directory: %s", test_dir);

    // 清理可能存在的旧目录
    cleanup_dir(test_dir);

    // 创建测试目录
    ppdb_error_t err = ensure_directory(test_dir);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create test directory: %s", test_dir);
        return err;
    }

    // 创建WAL配置
    ppdb_wal_config_t config = {
        .dir_path = test_dir,
        .segment_size = 4096,
        .sync_write = true
    };

    // 创建WAL实例
    ppdb_wal_t* wal = NULL;
    err = ppdb_wal_create(&config, &wal);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create WAL: %d", err);
        cleanup_dir(test_dir);
        return err;
    }

    // 写入一些记录
    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_wal_write(wal, PPDB_WAL_RECORD_PUT,
                        (uint8_t*)key, strlen(key) + 1,  // 包含null终止符
                        (uint8_t*)value, strlen(value) + 1);  // 包含null终止符
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to write to WAL: %d", err);
        ppdb_wal_destroy(wal);
        cleanup_dir(test_dir);
        return err;
    }

    // 删除一个记录
    const char* delete_key = "delete_key";
    err = ppdb_wal_write(wal, PPDB_WAL_RECORD_DELETE,
                        (uint8_t*)delete_key, strlen(delete_key) + 1,  // 包含null终止符
                        NULL, 0);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to write delete record to WAL: %d", err);
        ppdb_wal_destroy(wal);
        cleanup_dir(test_dir);
        return err;
    }

    // 创建MemTable用于恢复测试
    ppdb_memtable_t* memtable = NULL;
    err = ppdb_memtable_create(1024 * 1024, &memtable);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create memtable: %d", err);
        ppdb_wal_destroy(wal);
        cleanup_dir(test_dir);
        return err;
    }

    // 先在memtable中插入要删除的键
    err = ppdb_memtable_put(memtable, (uint8_t*)delete_key, strlen(delete_key) + 1,
                           (uint8_t*)"dummy", strlen("dummy") + 1);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to put delete key to memtable: %d", err);
        ppdb_memtable_destroy(memtable);
        ppdb_wal_destroy(wal);
        cleanup_dir(test_dir);
        return err;
    }

    // 从WAL恢复数据
    err = ppdb_wal_recover(wal, memtable);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to recover from WAL: %d", err);
        ppdb_memtable_destroy(memtable);
        ppdb_wal_destroy(wal);
        cleanup_dir(test_dir);
        return err;
    }

    // 验证恢复的数据
    size_t value_size = 256;  // 设置足够大的缓冲区
    uint8_t value_buf[256];
    err = ppdb_memtable_get(memtable, (uint8_t*)key, strlen(key) + 1,  // 包含null终止符
                           value_buf, &value_size);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to get recovered value: %d", err);
        ppdb_memtable_destroy(memtable);
        ppdb_wal_destroy(wal);
        cleanup_dir(test_dir);
        return err;
    }

    if (value_size != strlen(value) + 1 ||  // 包含null终止符
        memcmp(value_buf, value, value_size) != 0) {
        ppdb_log_error("Recovered value does not match original");
        ppdb_memtable_destroy(memtable);
        ppdb_wal_destroy(wal);
        cleanup_dir(test_dir);
        return PPDB_ERR_CORRUPTED;
    }

    // 验证删除的记录
    err = ppdb_memtable_get(memtable, (uint8_t*)delete_key, strlen(delete_key) + 1,  // 包含null终止符
                           value_buf, &value_size);
    if (err != PPDB_ERR_NOT_FOUND) {
        ppdb_log_error("Deleted key still exists: %d", err);
        ppdb_memtable_destroy(memtable);
        ppdb_wal_destroy(wal);
        cleanup_dir(test_dir);
        return PPDB_ERR_CORRUPTED;
    }

    // 清理资源
    ppdb_memtable_destroy(memtable);
    ppdb_wal_destroy(wal);
    cleanup_dir(test_dir);

    ppdb_log_info("Basic WAL test passed");
    return PPDB_OK;
}

// 并发测试
ppdb_error_t test_wal_concurrent() {
    ppdb_log_info("Running concurrent WAL test...");

    char test_dir[MAX_PATH_LENGTH];
    if (!getcwd(test_dir, sizeof(test_dir))) {
        ppdb_log_error("Failed to get current working directory: %s", strerror(errno));
        return PPDB_ERR_IO;
    }
    ppdb_log_info("Current working directory: %s", test_dir);

    strcat(test_dir, "\\test_wal_concurrent");
    ppdb_log_info("WAL directory: %s", test_dir);

    // 清理可能存在的旧目录
    cleanup_dir(test_dir);

    // 创建测试目录
    ppdb_error_t err = ensure_directory(test_dir);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create test directory: %s", test_dir);
        return err;
    }

    // 创建WAL配置
    ppdb_wal_config_t config = {
        .dir_path = test_dir,
        .segment_size = 4096,
        .sync_write = true
    };

    // 创建WAL实例
    ppdb_wal_t* wal = NULL;
    err = ppdb_wal_create(&config, &wal);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create WAL: %d", err);
        cleanup_dir(test_dir);
        return err;
    }

    // 创建MemTable用于恢复测试
    ppdb_memtable_t* memtable = NULL;
    err = ppdb_memtable_create(1024 * 1024, &memtable);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create memtable: %d", err);
        ppdb_wal_destroy(wal);
        cleanup_dir(test_dir);
        return err;
    }

    // 写入一些并发记录
    const int num_records = 100;
    for (int i = 0; i < num_records; i++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        err = ppdb_wal_write(wal, PPDB_WAL_RECORD_PUT,
                            (uint8_t*)key, strlen(key) + 1,
                            (uint8_t*)value, strlen(value) + 1);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to write concurrent record to WAL: %d", err);
            ppdb_memtable_destroy(memtable);
            ppdb_wal_destroy(wal);
            cleanup_dir(test_dir);
            return err;
        }
    }

    // 从WAL恢复数据
    err = ppdb_wal_recover(wal, memtable);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to recover from WAL: %d", err);
        ppdb_memtable_destroy(memtable);
        ppdb_wal_destroy(wal);
        cleanup_dir(test_dir);
        return err;
    }

    // 验证恢复的数据
    for (int i = 0; i < num_records; i++) {
        char key[32], expected_value[32];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(expected_value, sizeof(expected_value), "value_%d", i);

        uint8_t value_buf[256];
        size_t value_size = sizeof(value_buf);
        err = ppdb_memtable_get(memtable, (uint8_t*)key, strlen(key) + 1,
                               value_buf, &value_size);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to get recovered value for key %s: %d", key, err);
            ppdb_memtable_destroy(memtable);
            ppdb_wal_destroy(wal);
            cleanup_dir(test_dir);
            return err;
        }

        if (value_size != strlen(expected_value) + 1 ||
            memcmp(value_buf, expected_value, value_size) != 0) {
            ppdb_log_error("Recovered value does not match original for key %s", key);
            ppdb_memtable_destroy(memtable);
            ppdb_wal_destroy(wal);
            cleanup_dir(test_dir);
            return PPDB_ERR_CORRUPTED;
        }
    }

    // 清理资源
    ppdb_memtable_destroy(memtable);
    ppdb_wal_destroy(wal);
    cleanup_dir(test_dir);

    ppdb_log_info("Concurrent WAL test passed");
    return PPDB_OK;
} 