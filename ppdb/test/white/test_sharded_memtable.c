#include <cosmopolitan.h>
#include "test_framework.h"
#include "test_plan.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/kvstore_memtable.h"

#define NUM_THREADS 4
#define NUM_OPS 1000
#define MAX_KEY_SIZE 64
#define MAX_VALUE_SIZE 128

// 生成测试数据
static void generate_test_data(char* key, size_t key_size, 
                             char* value, size_t value_size,
                             int thread_id, int op_id) {
    snprintf(key, key_size, "key_%d_%d", thread_id, op_id);
    snprintf(value, value_size, "value_%d_%d", thread_id, op_id);
}

// 基本操作测试
static int test_basic_ops(void) {
    ppdb_log_info("Testing basic operations...");

    // 创建分片内存表
    ppdb_sharded_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_sharded_memtable_create(&table, 4);
    TEST_ASSERT(err == PPDB_OK, "Create sharded memtable failed");
    TEST_ASSERT(table != NULL, "Memtable pointer is NULL");

    // 测试插入和获取
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    err = ppdb_sharded_memtable_put(table, test_key, strlen(test_key) + 1,
                                   test_value, strlen(test_value) + 1);
    TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");

    // 获取值
    char value[32] = {0};
    size_t value_size = sizeof(value);
    err = ppdb_sharded_memtable_get(table, test_key, strlen(test_key) + 1,
                                   value, &value_size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value");
    TEST_ASSERT(value_size == strlen(test_value) + 1, "Value size mismatch");
    TEST_ASSERT(memcmp(value, test_value, value_size) == 0, "Value content mismatch");

    // 测试删除
    err = ppdb_sharded_memtable_delete(table, test_key, strlen(test_key) + 1);
    TEST_ASSERT(err == PPDB_OK, "Failed to delete key-value pair");

    // 验证删除
    err = ppdb_sharded_memtable_get(table, test_key, strlen(test_key) + 1,
                                   value, &value_size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key still exists after deletion");

    // 销毁内存表
    ppdb_sharded_memtable_destroy(table);
    return 0;
}

// 分片均衡性测试
static int test_shard_distribution(void) {
    ppdb_log_info("Testing shard distribution...");

    // 创建分片内存表
    ppdb_sharded_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_sharded_memtable_create(&table, 4);
    TEST_ASSERT(err == PPDB_OK, "Create sharded memtable failed");
    TEST_ASSERT(table != NULL, "Memtable pointer is NULL");

    // 插入一些数据
    char key[32], value[32];
    for (int i = 0; i < 100; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        err = ppdb_sharded_memtable_put(table, key, strlen(key) + 1,
                                       value, strlen(value) + 1);
        TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    }

    // 检查每个分片是否都有数据
    int empty_shards = 0;
    for (size_t i = 0; i < table->shard_count; i++) {
        size_t shard_size = ppdb_memtable_size_basic(table->shards[i]);
        if (shard_size == 0) {
            empty_shards++;
        }
    }
    TEST_ASSERT(empty_shards == 0, "Found empty shards");

    // 销毁内存表
    ppdb_sharded_memtable_destroy(table);
    return 0;
}

// 并发操作测试
typedef struct {
    ppdb_sharded_memtable_t* table;
    int thread_id;
} thread_arg_t;

static void* concurrent_worker(void* arg) {
    thread_arg_t* thread_arg = (thread_arg_t*)arg;
    ppdb_sharded_memtable_t* table = thread_arg->table;
    int thread_id = thread_arg->thread_id;

    char key[MAX_KEY_SIZE];
    char value[MAX_VALUE_SIZE];
    char retrieved_value[MAX_VALUE_SIZE];

    for (int i = 0; i < NUM_OPS; i++) {
        generate_test_data(key, sizeof(key), value, sizeof(value), thread_id, i);

        // 插入
        ppdb_error_t err = ppdb_sharded_memtable_put(table, key, strlen(key) + 1,
                                                    value, strlen(value) + 1);
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d: Failed to put key-value pair", thread_id);
            return NULL;
        }

        // 获取
        size_t value_size = sizeof(retrieved_value);
        err = ppdb_sharded_memtable_get(table, key, strlen(key) + 1,
                                       retrieved_value, &value_size);
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d: Failed to get value", thread_id);
            return NULL;
        }
        if (value_size != strlen(value) + 1 || memcmp(retrieved_value, value, value_size) != 0) {
            ppdb_log_error("Thread %d: Value mismatch", thread_id);
            return NULL;
        }

        // 删除
        err = ppdb_sharded_memtable_delete(table, key, strlen(key) + 1);
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d: Failed to delete key-value pair", thread_id);
            return NULL;
        }
    }

    return NULL;
}

static int test_concurrent_ops(void) {
    ppdb_log_info("Testing concurrent operations...");

    // 创建分片内存表
    ppdb_sharded_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_sharded_memtable_create(&table, 4);
    TEST_ASSERT(err == PPDB_OK, "Create sharded memtable failed");
    TEST_ASSERT(table != NULL, "Memtable pointer is NULL");

    // 创建线程
    pthread_t threads[NUM_THREADS];
    thread_arg_t thread_args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].table = table;
        thread_args[i].thread_id = i;
        if (pthread_create(&threads[i], NULL, concurrent_worker, &thread_args[i]) != 0) {
            TEST_ASSERT(0, "Failed to create thread %d", i);
        }
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        void* result;
        if (pthread_join(threads[i], &result) != 0) {
            TEST_ASSERT(0, "Failed to join thread %d", i);
        }
        TEST_ASSERT(result == NULL, "Thread %d failed", i);
    }

    // 销毁内存表
    ppdb_sharded_memtable_destroy(table);
    return 0;
}

// 迭代器测试
static int test_iterator(void) {
    ppdb_log_info("Testing iterator...");

    // 创建分片内存表
    ppdb_sharded_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_sharded_memtable_create(&table, 4);
    TEST_ASSERT(err == PPDB_OK, "Create sharded memtable failed");
    TEST_ASSERT(table != NULL, "Memtable pointer is NULL");

    // 插入有序的键值对
    const int num_pairs = 100;
    for (int i = 0; i < num_pairs; i++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "iter_key_%03d", i);
        snprintf(value, sizeof(value), "iter_value_%03d", i);
        printf("Inserted key: %s, value: %s\n", key, value);
        err = ppdb_sharded_memtable_put(table, key, strlen(key) + 1,
                                       value, strlen(value) + 1);
        TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    }

    // 创建迭代器
    ppdb_iterator_t* iter = NULL;
    err = ppdb_sharded_memtable_iterator_create(table, &iter);
    TEST_ASSERT(err == PPDB_OK, "Failed to create iterator");
    TEST_ASSERT(iter != NULL, "Iterator pointer is NULL");

    // 验证迭代顺序
    int count = 0;
    bool has_next = iter->valid(iter);
    printf("Initial valid: %d\n", has_next);

    while (has_next) {
        ppdb_kv_pair_t pair = {0};
        err = iter->get(iter, &pair);
        TEST_ASSERT(err == PPDB_OK, "Failed to get key-value pair");

        char expected_key[32], expected_value[32];
        snprintf(expected_key, sizeof(expected_key), "iter_key_%03d", count);
        snprintf(expected_value, sizeof(expected_value), "iter_value_%03d", count);

        printf("Count: %d\n", count);
        printf("Expected key: %s (%zu bytes)\n", expected_key, strlen(expected_key));
        printf("Actual key: %.*s (%zu bytes)\n", (int)pair.key_size, (char*)pair.key, pair.key_size);
        printf("Expected value: %s (%zu bytes)\n", expected_value, strlen(expected_value));
        printf("Actual value: %.*s (%zu bytes)\n\n", (int)pair.value_size, (char*)pair.value, pair.value_size);

        TEST_ASSERT(pair.key_size == strlen(expected_key) + 1, "Key size mismatch");
        TEST_ASSERT(memcmp(pair.key, expected_key, pair.key_size - 1) == 0, "Key content mismatch");
        TEST_ASSERT(pair.value_size == strlen(expected_value) + 1, "Value size mismatch");
        TEST_ASSERT(memcmp(pair.value, expected_value, pair.value_size - 1) == 0, "Value content mismatch");

        count++;
        has_next = iter->next(iter);
        printf("Next valid: %d\n", has_next);
    }

    printf("Final count: %d, expected: %d\n", count, num_pairs);
    TEST_ASSERT(count == num_pairs, "Iterator count mismatch");

    // 销毁迭代器和内存表
    ppdb_iterator_destroy(iter);
    ppdb_sharded_memtable_destroy(table);
    return 0;
}

// 测试套件
test_suite_t sharded_memtable_suite = {
    .name = "Sharded Memtable",
    .cases = (test_case_t[]){
        {"test_basic_ops", test_basic_ops},
        {"test_shard_distribution", test_shard_distribution},
        {"test_concurrent_ops", test_concurrent_ops},
        {"test_iterator", test_iterator},
        {NULL, NULL}  // 结束标记
    }
};

// 注册测试套件
void register_sharded_memtable_tests(void) {
    register_test_suite(&sharded_memtable_suite);
}

// 主函数
int main(void) {
    register_sharded_memtable_tests();
    return run_all_tests();
}