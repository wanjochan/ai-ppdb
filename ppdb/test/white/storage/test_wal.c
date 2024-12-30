#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_logger.h"

#define TEST_DIR "./tmp_test_wal"
#define MAX_KEY_SIZE 64
#define MAX_VALUE_SIZE 128

// WAL资源清理函数
static void cleanup_wal(ppdb_wal_t* wal) {
    if (wal) {
        ppdb_wal_close(wal);
        ppdb_wal_destroy(wal);
    }
}

// 基本操作测试
void test_basic_ops(void) {
    // 创建WAL
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir = TEST_DIR,
        .sync_mode = PPDB_SYNC_MODE_ASYNC
    };
    
    ppdb_error_t err = ppdb_wal_create(&config, &wal);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_NOT_NULL(wal);
    
    // 写入测试数据
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    
    err = ppdb_wal_write(wal, PPDB_WAL_RECORD_PUT,
        test_key, strlen(test_key),
        test_value, strlen(test_value));
    ASSERT_EQ(err, PPDB_OK);
    
    // 创建memtable验证
    ppdb_memtable_t* table = NULL;
    ppdb_memtable_config_t table_config = {
        .size = 1024 * 1024,
        .dir = TEST_DIR
    };
    
    err = ppdb_memtable_create(&table_config, &table);
    ASSERT_EQ(err, PPDB_OK);
    
    // 恢复WAL到memtable
    err = ppdb_wal_recover(wal, table);
    ASSERT_EQ(err, PPDB_OK);
    
    // 验证数据
    const void* value;
    size_t value_size;
    err = ppdb_memtable_get(table, test_key, strlen(test_key), &value, &value_size);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_EQ(value_size, strlen(test_value));
    ASSERT_MEM_EQ(value, test_value, value_size);
    
    ppdb_memtable_destroy(table);
    cleanup_wal(wal);
}

// 恢复测试
void test_recovery(void) {
    // 创建WAL
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir = TEST_DIR,
        .sync_mode = PPDB_SYNC_MODE_ASYNC
    };
    
    ppdb_error_t err = ppdb_wal_create(&config, &wal);
    ASSERT_EQ(err, PPDB_OK);
    
    // 写入多条记录
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    const int num_records = 3;
    
    for (int i = 0; i < num_records; i++) {
        err = ppdb_wal_write(wal, PPDB_WAL_RECORD_PUT,
            keys[i], strlen(keys[i]),
            values[i], strlen(values[i]));
        ASSERT_EQ(err, PPDB_OK);
    }
    
    // 关闭WAL
    ppdb_wal_close(wal);
    
    // 重新打开WAL
    err = ppdb_wal_create(&config, &wal);
    ASSERT_EQ(err, PPDB_OK);
    
    // 创建memtable验证
    ppdb_memtable_t* table = NULL;
    ppdb_memtable_config_t table_config = {
        .size = 1024 * 1024,
        .dir = TEST_DIR
    };
    
    err = ppdb_memtable_create(&table_config, &table);
    ASSERT_EQ(err, PPDB_OK);
    
    // 恢复WAL到memtable
    err = ppdb_wal_recover(wal, table);
    ASSERT_EQ(err, PPDB_OK);
    
    // 验证所有记录
    for (int i = 0; i < num_records; i++) {
        const void* value;
        size_t value_size;
        err = ppdb_memtable_get(table, keys[i], strlen(keys[i]), &value, &value_size);
        ASSERT_EQ(err, PPDB_OK);
        ASSERT_EQ(value_size, strlen(values[i]));
        ASSERT_MEM_EQ(value, values[i], value_size);
    }
    
    ppdb_memtable_destroy(table);
    cleanup_wal(wal);
}

// 性能测试
void test_performance(void) {
    // 创建WAL
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir = TEST_DIR,
        .sync_mode = PPDB_SYNC_MODE_ASYNC
    };
    
    ppdb_error_t err = ppdb_wal_create(&config, &wal);
    ASSERT_EQ(err, PPDB_OK);

    // 批量写入测试
    #define BATCH_SIZE 1000
    char key[32], value[128];
    
    // 测量写入性能
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < BATCH_SIZE; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        
        err = ppdb_wal_write(wal, PPDB_WAL_RECORD_PUT,
            key, strlen(key),
            value, strlen(value));
        ASSERT_EQ(err, PPDB_OK);
    }
    
    // 强制同步
    err = ppdb_wal_sync(wal);
    ASSERT_EQ(err, PPDB_OK);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // 计算性能指标
    double duration = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    double ops_per_sec = BATCH_SIZE / duration;
    
    // 验证性能达标（至少1000 ops/s）
    ASSERT_GT(ops_per_sec, 1000.0);
    
    cleanup_wal(wal);
}

int main(void) {
    TEST_INIT("Write-Ahead Log Test");
    
    RUN_TEST(test_basic_ops);
    RUN_TEST(test_recovery);
    RUN_TEST(test_performance);
    
    TEST_SUMMARY();
    return TEST_RESULT();
} 