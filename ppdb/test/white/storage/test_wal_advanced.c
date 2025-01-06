#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "test/test_utils.h"
#include "../../src/internal/base.h"

// 测试并发写入
void test_concurrent_write(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 4,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 创建多个线程进行写入
    #define NUM_THREADS 4
    #define WRITES_PER_THREAD 100

    ppdb_base_thread_t* threads[NUM_THREADS];
    
    void* thread_func(void* arg) {
        int thread_id = *(int*)arg;
        char key[32];
        char value[32];

        for (int i = 0; i < WRITES_PER_THREAD; i++) {
            snprintf(key, sizeof(key), "key_%d_%d", thread_id, i);
            snprintf(value, sizeof(value), "value_%d_%d", thread_id, i);
            ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));
        }
        return NULL;
    }

    int thread_ids[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        ppdb_base_thread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        ppdb_base_thread_join(threads[i], NULL);
    }

    // 验证写入的数据
    ppdb_memtable_t* memtable = NULL;
    ASSERT_OK(ppdb_memtable_create(&memtable));
    ASSERT_OK(ppdb_wal_recover(wal, memtable));

    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < WRITES_PER_THREAD; i++) {
            char key[32];
            char expected_value[32];
            snprintf(key, sizeof(key), "key_%d_%d", t, i);
            snprintf(expected_value, sizeof(expected_value), "value_%d_%d", t, i);

            void* value = NULL;
            size_t value_size = 0;
            ASSERT_OK(ppdb_memtable_get(memtable, key, strlen(key), &value, &value_size));
            ASSERT_EQ(value_size, strlen(expected_value));
            ASSERT_EQ(memcmp(value, expected_value, value_size), 0);
            free(value);
        }
    }

    // 清理
    ppdb_destroy(memtable);
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

// 测试批量写入
void test_batch_write(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 准备批量写入数据
    #define BATCH_SIZE 100
    ppdb_write_batch_t batch;
    ppdb_write_op_t ops[BATCH_SIZE];
    char keys[BATCH_SIZE][32];
    char values[BATCH_SIZE][32];

    for (int i = 0; i < BATCH_SIZE; i++) {
        snprintf(keys[i], sizeof(keys[i]), "batch_key_%d", i);
        snprintf(values[i], sizeof(values[i]), "batch_value_%d", i);
        ops[i].key = keys[i];
        ops[i].key_size = strlen(keys[i]);
        ops[i].value = values[i];
        ops[i].value_size = strlen(values[i]);
    }

    batch.ops = ops;
    batch.count = BATCH_SIZE;

    // 执行批量写入
    ASSERT_OK(ppdb_wal_write_batch(wal, &batch));

    // 验证写入的数据
    ppdb_memtable_t* memtable = NULL;
    ASSERT_OK(ppdb_memtable_create(&memtable));
    ASSERT_OK(ppdb_wal_recover(wal, memtable));

    for (int i = 0; i < BATCH_SIZE; i++) {
        void* value = NULL;
        size_t value_size = 0;
        ASSERT_OK(ppdb_memtable_get(memtable, keys[i], strlen(keys[i]), 
                                   &value, &value_size));
        ASSERT_EQ(value_size, strlen(values[i]));
        ASSERT_EQ(memcmp(value, values[i], value_size), 0);
        free(value);
    }

    // 清理
    ppdb_destroy(memtable);
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

// 测试压缩优化
void test_compaction(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 写入足够多的数据以触发压缩
    const char* key = "test_key";
    const char* value = "test_value";
    for (int i = 0; i < 1000; i++) {
        ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));
    }

    // 执行压缩
    ASSERT_OK(ppdb_wal_compact(wal));

    // 验证段数量不超过限制
    ASSERT_LE(wal->segment_count, config.max_segments);

    // 验证数据完整性
    ppdb_memtable_t* memtable = NULL;
    ASSERT_OK(ppdb_memtable_create(&memtable));
    ASSERT_OK(ppdb_wal_recover(wal, memtable));

    void* read_value = NULL;
    size_t value_size = 0;
    ASSERT_OK(ppdb_memtable_get(memtable, key, strlen(key), &read_value, &value_size));
    ASSERT_EQ(value_size, strlen(value));
    ASSERT_EQ(memcmp(read_value, value, value_size), 0);
    free(read_value);

    // 清理
    ppdb_destroy(memtable);
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

// 测试性能优化
void test_performance(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 4,
        .sync_write = false  // 关闭同步写入以测试性能
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 测试大量小记录的写入性能
    #define NUM_SMALL_RECORDS 10000
    const char* small_key = "key";
    const char* small_value = "value";
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_SMALL_RECORDS; i++) {
        ASSERT_OK(ppdb_wal_write(wal, small_key, strlen(small_key), 
                                small_value, strlen(small_value)));
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double small_records_time = (end.tv_sec - start.tv_sec) + 
                              (end.tv_nsec - start.tv_nsec) / 1e9;

    // 测试大记录的写入性能
    #define NUM_LARGE_RECORDS 100
    #define LARGE_RECORD_SIZE 4000
    char* large_value = malloc(LARGE_RECORD_SIZE);
    memset(large_value, 'x', LARGE_RECORD_SIZE);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_LARGE_RECORDS; i++) {
        ASSERT_OK(ppdb_wal_write(wal, small_key, strlen(small_key), 
                                large_value, LARGE_RECORD_SIZE));
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double large_records_time = (end.tv_sec - start.tv_sec) + 
                              (end.tv_nsec - start.tv_nsec) / 1e9;

    // 验证性能指标
    double small_records_per_sec = NUM_SMALL_RECORDS / small_records_time;
    double large_records_per_sec = NUM_LARGE_RECORDS / large_records_time;
    double small_mb_per_sec = (NUM_SMALL_RECORDS * (strlen(small_key) + strlen(small_value))) 
                             / (1024 * 1024) / small_records_time;
    double large_mb_per_sec = (NUM_LARGE_RECORDS * (strlen(small_key) + LARGE_RECORD_SIZE)) 
                             / (1024 * 1024) / large_records_time;

    printf("Small records: %.2f ops/sec, %.2f MB/sec\n", 
           small_records_per_sec, small_mb_per_sec);
    printf("Large records: %.2f ops/sec, %.2f MB/sec\n", 
           large_records_per_sec, large_mb_per_sec);

    // 设置最低性能要求
    ASSERT_GT(small_records_per_sec, 1000);  // 至少每秒1000次小记录写入
    ASSERT_GT(large_records_per_sec, 10);    // 至少每秒10次大记录写入
    ASSERT_GT(small_mb_per_sec, 1);         // 至少每秒1MB小记录吞吐量
    ASSERT_GT(large_mb_per_sec, 10);        // 至少每秒10MB大记录吞吐量

    // 清理
    free(large_value);
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

int main(void) {
    TEST_INIT();
    
    RUN_TEST(test_concurrent_write);
    RUN_TEST(test_batch_write);
    RUN_TEST(test_compaction);
    RUN_TEST(test_performance);
    
    TEST_SUMMARY();
    return 0;
} 