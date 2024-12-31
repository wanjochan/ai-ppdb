#include "kvstore/internal/kvstore_wal.h"
#include "test/white/test_framework.h"
#include <cosmopolitan.h>

#define TEST_DIR "tmp_test/wal"
#define TEST_KEY "test_key"
#define TEST_VALUE "test_value"

// 清理函数
static void cleanup_wal(ppdb_wal_t* wal) {
    if (wal) {
        ppdb_wal_close(wal);
        ppdb_wal_destroy(wal);
    }
}

// 基本操作测试
static int test_basic_ops(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = TEST_DIR,
        .sync_write = false,
        .use_buffer = true,
        .buffer_size = 4096,
        .segment_size = 1024 * 1024,  // 1MB
        .max_segments = 10,
        .sync_on_write = false,
        .enable_compression = false
    };

    ppdb_error_t err = ppdb_wal_create(&config, &wal);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_NOT_NULL(wal);

    // 写入测试数据
    err = ppdb_wal_write(wal, TEST_KEY, strlen(TEST_KEY),
                         TEST_VALUE, strlen(TEST_VALUE));
    ASSERT_EQ(err, PPDB_OK);

    // 检查WAL大小
    size_t wal_size = ppdb_wal_size(wal);
    ASSERT_GT(wal_size, 0);

    // 同步WAL
    err = ppdb_wal_sync(wal);
    ASSERT_EQ(err, PPDB_OK);

    cleanup_wal(wal);
    return 0;
}

// 恢复测试
static int test_recovery(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = TEST_DIR,
        .sync_write = false,
        .use_buffer = true,
        .buffer_size = 4096,
        .segment_size = 1024 * 1024,  // 1MB
        .max_segments = 10,
        .sync_on_write = false,
        .enable_compression = false
    };

    ppdb_error_t err = ppdb_wal_create(&config, &wal);
    ASSERT_EQ(err, PPDB_OK);

    // 写入多条记录
    const int num_records = 100;
    for (int i = 0; i < num_records; i++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);

        err = ppdb_wal_write(wal, key, strlen(key), value, strlen(value));
        ASSERT_EQ(err, PPDB_OK);
    }

    // 同步并记录大小
    err = ppdb_wal_sync(wal);
    ASSERT_EQ(err, PPDB_OK);

    size_t wal_size = ppdb_wal_size(wal);
    ASSERT_GT(wal_size, 0);

    // 关闭WAL
    cleanup_wal(wal);

    // 重新打开WAL
    err = ppdb_wal_create(&config, &wal);
    ASSERT_EQ(err, PPDB_OK);

    // 检查大小是否一致
    size_t new_wal_size = ppdb_wal_size(wal);
    ASSERT_EQ(new_wal_size, wal_size);

    cleanup_wal(wal);
    return 0;
}

// 性能测试
static int test_performance(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = TEST_DIR,
        .sync_write = false,
        .use_buffer = true,
        .buffer_size = 4096,
        .segment_size = 1024 * 1024,  // 1MB
        .max_segments = 10,
        .sync_on_write = false,
        .enable_compression = false
    };

    ppdb_error_t err = ppdb_wal_create(&config, &wal);
    ASSERT_EQ(err, PPDB_OK);

    // 写入大量记录并测量性能
    const int num_records = 10000;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_records; i++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "perf_key_%d", i);
        snprintf(value, sizeof(value), "perf_value_%d", i);

        err = ppdb_wal_write(wal, key, strlen(key), value, strlen(value));
        ASSERT_EQ(err, PPDB_OK);
    }

    err = ppdb_wal_sync(wal);
    ASSERT_EQ(err, PPDB_OK);

    clock_gettime(CLOCK_MONOTONIC, &end);

    // 计算性能指标
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_nsec - start.tv_nsec) / 1e9;
    double ops_per_sec = num_records / elapsed;

    // 确保性能达标（至少1000 ops/s）
    ASSERT_GT(ops_per_sec, 1000.0);

    cleanup_wal(wal);
    return 0;
}

int main(void) {
    RUN_TEST(test_basic_ops);
    RUN_TEST(test_recovery);
    RUN_TEST(test_performance);
    return 0;
} 