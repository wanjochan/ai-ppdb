#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "test_framework.h"
#include "test_plan.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"
#include "src/kvstore/internal/kvstore_wal.h"
#include "src/kvstore/internal/kvstore_memtable.h"

#define TEST_DIR "./tmp_test_wal"
#define MAX_KEY_SIZE 64
#define MAX_VALUE_SIZE 128

// WAL资源清理函数
static void cleanup_wal(void* ptr) {
    ppdb_wal_t* wal = (ppdb_wal_t*)ptr;
    if (wal) {
        ppdb_wal_close_lockfree(wal);
        ppdb_wal_destroy_lockfree(wal);
    }
}

// 基本写入测试
static int test_wal_basic_write(void) {
    // 创建WAL
    ppdb_wal_t* wal;
    ppdb_wal_config_t config = {
        .dir = TEST_DIR,
        .sync_mode = PPDB_SYNC_MODE_ASYNC
    };
    
    ppdb_error_t err = ppdb_wal_create_lockfree(&config, &wal);
    TEST_ASSERT_OK(err, "Failed to create WAL");
    TEST_TRACK(wal, "wal", cleanup_wal);
    
    // 写入测试数据
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    
    err = ppdb_wal_write_lockfree(wal, PPDB_WAL_RECORD_PUT,
        test_key, strlen(test_key),
        test_value, strlen(test_value));
    TEST_ASSERT_OK(err, "Failed to write to WAL");
    
    // 创建memtable验证
    ppdb_memtable_t* table;
    ppdb_memtable_config_t table_config = {
        .size = 1024 * 1024,
        .dir = TEST_DIR
    };
    
    err = ppdb_memtable_create(&table_config, &table);
    TEST_ASSERT_OK(err, "Failed to create memtable");
    
    // 恢复WAL到memtable
    err = ppdb_wal_recover_lockfree(wal, table);
    TEST_ASSERT_OK(err, "Failed to recover WAL");
    
    // 验证数据
    const void* value;
    size_t value_size;
    err = ppdb_memtable_get(table, test_key, strlen(test_key), &value, &value_size);
    TEST_ASSERT_OK(err, "Failed to get value from memtable");
    TEST_ASSERT(value_size == strlen(test_value), "Value size mismatch");
    TEST_ASSERT(memcmp(value, test_value, value_size) == 0, "Value content mismatch");
    
    ppdb_memtable_destroy(table);
    return 0;
}

// 删除记录测试
static int test_wal_delete(void) {
    // 创建WAL
    ppdb_wal_t* wal;
    ppdb_wal_config_t config = {
        .dir = TEST_DIR,
        .sync_mode = PPDB_SYNC_MODE_ASYNC
    };
    
    ppdb_error_t err = ppdb_wal_create_lockfree(&config, &wal);
    TEST_ASSERT_OK(err, "Failed to create WAL");
    TEST_TRACK(wal, "wal", cleanup_wal);
    
    // 写入然后删除
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    
    err = ppdb_wal_write_lockfree(wal, PPDB_WAL_RECORD_PUT,
        test_key, strlen(test_key),
        test_value, strlen(test_value));
    TEST_ASSERT_OK(err, "Failed to write to WAL");
    
    err = ppdb_wal_write_lockfree(wal, PPDB_WAL_RECORD_DELETE,
        test_key, strlen(test_key), NULL, 0);
    TEST_ASSERT_OK(err, "Failed to delete from WAL");
    
    // 创建memtable验证
    ppdb_memtable_t* table;
    ppdb_memtable_config_t table_config = {
        .size = 1024 * 1024,
        .dir = TEST_DIR
    };
    
    err = ppdb_memtable_create(&table_config, &table);
    TEST_ASSERT_OK(err, "Failed to create memtable");
    
    // 恢复WAL到memtable
    err = ppdb_wal_recover_lockfree(wal, table);
    TEST_ASSERT_OK(err, "Failed to recover WAL");
    
    // 验证数据被删除
    const void* value;
    size_t value_size;
    err = ppdb_memtable_get(table, test_key, strlen(test_key), &value, &value_size);
    TEST_ASSERT(err == PPDB_NOT_FOUND, "Key should be deleted");
    
    ppdb_memtable_destroy(table);
    return 0;
}

// 恢复测试
static int test_wal_recovery(void) {
    // 创建WAL
    ppdb_wal_t* wal;
    ppdb_wal_config_t config = {
        .dir = TEST_DIR,
        .sync_mode = PPDB_SYNC_MODE_ASYNC
    };
    
    ppdb_error_t err = ppdb_wal_create_lockfree(&config, &wal);
    TEST_ASSERT_OK(err, "Failed to create WAL");
    TEST_TRACK(wal, "wal", cleanup_wal);
    
    // 写入多条记录
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    const int num_records = 3;
    
    for (int i = 0; i < num_records; i++) {
        err = ppdb_wal_write_lockfree(wal, PPDB_WAL_RECORD_PUT,
            keys[i], strlen(keys[i]),
            values[i], strlen(values[i]));
        TEST_ASSERT_OK(err, "Failed to write record %d", i);
    }
    
    // 关闭WAL
    ppdb_wal_close_lockfree(wal);
    
    // 重新打开WAL
    err = ppdb_wal_create_lockfree(&config, &wal);
    TEST_ASSERT_OK(err, "Failed to reopen WAL");
    TEST_TRACK(wal, "wal", cleanup_wal);
    
    // 创建memtable验证
    ppdb_memtable_t* table;
    ppdb_memtable_config_t table_config = {
        .size = 1024 * 1024,
        .dir = TEST_DIR
    };
    
    err = ppdb_memtable_create(&table_config, &table);
    TEST_ASSERT_OK(err, "Failed to create memtable");
    
    // 恢复WAL到memtable
    err = ppdb_wal_recover_lockfree(wal, table);
    TEST_ASSERT_OK(err, "Failed to recover WAL");
    
    // 验证所有记录
    for (int i = 0; i < num_records; i++) {
        const void* value;
        size_t value_size;
        err = ppdb_memtable_get(table, keys[i], strlen(keys[i]), &value, &value_size);
        TEST_ASSERT_OK(err, "Failed to get record %d", i);
        TEST_ASSERT(value_size == strlen(values[i]), "Value size mismatch for record %d", i);
        TEST_ASSERT(memcmp(value, values[i], value_size) == 0, 
            "Value content mismatch for record %d", i);
    }
    
    ppdb_memtable_destroy(table);
    return 0;
}

// 注册WAL测试
void register_wal_tests(void) {
    static const test_case_t cases[] = {
        {
            .name = "test_wal_basic_write",
            .fn = test_wal_basic_write,
            .timeout_seconds = 30,
            .skip = false,
            .description = "测试WAL基本写入功能"
        },
        {
            .name = "test_wal_delete",
            .fn = test_wal_delete,
            .timeout_seconds = 30,
            .skip = false,
            .description = "测试WAL删除记录功能"
        },
        {
            .name = "test_wal_recovery",
            .fn = test_wal_recovery,
            .timeout_seconds = 30,
            .skip = false,
            .description = "测试WAL恢复功能"
        }
    };
    
    static const test_suite_t suite = {
        .name = "WAL Tests",
        .cases = cases,
        .num_cases = sizeof(cases) / sizeof(cases[0]),
        .setup = NULL,
        .teardown = NULL
    };
    
    run_test_suite(&suite);
}