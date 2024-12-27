#include <cosmopolitan.h>
#include "test_framework.h"
#include <ppdb/wal.h>
#include <ppdb/error.h>
#include <ppdb/memtable.h>

// 基础文件系统操作测试
static int test_wal_fs_ops(void) {
    ppdb_log_info("Testing WAL filesystem operations...");
    
    // 测试目录创建
    const char* test_dir = "test_wal_fs";
    ppdb_error_t err = ppdb_ensure_directory(test_dir);
    TEST_ASSERT(err == PPDB_OK || err == PPDB_ERR_EXISTS, "Failed to create directory");
    
    // 测试WAL配置
    ppdb_wal_config_t config = {
        .dir_path = test_dir,
        .segment_size = 4096,
        .sync_write = true
    };
    
    // 测试WAL创建
    ppdb_wal_t* wal = NULL;
    err = ppdb_wal_create(&config, &wal);
    TEST_ASSERT(err == PPDB_OK, "Failed to create WAL");
    
    // 清理
    if (wal) {
        ppdb_wal_destroy(wal);
    }
    return 0;
}

// 基础写入测试
static int test_wal_write(void) {
    ppdb_log_info("Testing WAL write operations...");
    
    const char* test_dir = "test_wal_write";
    ppdb_wal_config_t config = {
        .dir_path = test_dir,
        .segment_size = 4096,
        .sync_write = true
    };
    
    ppdb_wal_t* wal = NULL;
    ppdb_error_t err = ppdb_wal_create(&config, &wal);
    TEST_ASSERT(err == PPDB_OK, "Failed to create WAL");
    
    // 测试单条记录写入
    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_wal_append(wal, key, strlen(key), value, strlen(value));
    TEST_ASSERT(err == PPDB_OK, "Failed to append to WAL");
    
    // 清理
    if (wal) {
        ppdb_wal_destroy(wal);
    }
    return 0;
}

// 恢复测试
static int test_wal_recovery(void) {
    ppdb_log_info("Testing WAL recovery...");
    
    const char* test_dir = "test_wal_recovery";
    ppdb_wal_config_t config = {
        .dir_path = test_dir,
        .segment_size = 4096,
        .sync_write = true
    };
    
    // 创建WAL并写入数据
    ppdb_wal_t* wal = NULL;
    ppdb_error_t err = ppdb_wal_create(&config, &wal);
    TEST_ASSERT(err == PPDB_OK, "Failed to create WAL");
    
    // 写入测试数据
    const char* key = "recovery_key";
    const char* value = "recovery_value";
    err = ppdb_wal_append(wal, key, strlen(key), value, strlen(value));
    TEST_ASSERT(err == PPDB_OK, "Failed to append to WAL");
    
    // 创建MemTable用于恢复
    ppdb_memtable_t* memtable = NULL;
    err = ppdb_memtable_create(4096, &memtable);
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    
    // 测试恢复
    err = ppdb_wal_recover(wal, memtable);
    TEST_ASSERT(err == PPDB_OK, "Failed to recover from WAL");
    
    // 验证恢复的数据
    size_t value_size = 0;
    char recovered_value[256] = {0};
    err = ppdb_memtable_get(memtable, key, strlen(key), recovered_value, sizeof(recovered_value), &value_size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get recovered value");
    TEST_ASSERT(strcmp(recovered_value, value) == 0, "Recovered value does not match");
    
    // 清理
    if (wal) {
        ppdb_wal_destroy(wal);
    }
    if (memtable) {
        ppdb_memtable_destroy(memtable);
    }
    return 0;
}

// WAL测试套件定义
static const test_case_t wal_test_cases[] = {
    {"fs_ops", test_wal_fs_ops},
    {"write", test_wal_write},
    {"recovery", test_wal_recovery}
};

// 导出WAL测试套件
const test_suite_t wal_suite = {
    .name = "WAL",
    .cases = wal_test_cases,
    .num_cases = sizeof(wal_test_cases) / sizeof(wal_test_cases[0])
}; 