#include <cosmopolitan.h>
#include "test_framework.h"
#include <ppdb/kvstore.h>
#include <ppdb/error.h>
#include <ppdb/logger.h>
#include <ppdb/fs.h>

// Forward declaration of worker thread function
static void* concurrent_worker(void* arg);

// Test KVStore create/close
static int test_kvstore_create_close(void) {
    ppdb_log_info("Testing KVStore create/close...");
    
    const char* test_dir = "test_kvstore_create.db";
    const char* wal_dir = "test_kvstore_create.db/wal";
    
    // Clean up test directories
    ppdb_log_info("Cleaning up test directories...");
    cleanup_test_dir(wal_dir);  // Clean up WAL directory first
    cleanup_test_dir(test_dir); // Then clean up parent directory
    
    // Wait for resources to be released
    usleep(500000);  // 500ms
    
    // Create KVStore
    ppdb_log_info("Creating KVStore configuration...");
    ppdb_kvstore_config_t config = {
        .dir_path = {0},
        .memtable_size = 4096,
        .mode = PPDB_MODE_LOCKED
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    config.dir_path[sizeof(config.dir_path) - 1] = '\0';  // Ensure null termination
    
    ppdb_log_info("Creating KVStore instance...");
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    TEST_ASSERT(err == PPDB_OK, "Failed to create KVStore: %s", ppdb_error_string(err));
    TEST_ASSERT(store != NULL, "KVStore pointer is NULL");
    
    // Wait for initialization to complete
    usleep(500000);  // 500ms
    
    // Verify directories exist
    ppdb_log_info("Verifying directories...");
    TEST_ASSERT(ppdb_fs_dir_exists(test_dir), "KVStore directory does not exist");
    TEST_ASSERT(ppdb_fs_dir_exists(wal_dir), "WAL directory does not exist");
    
    // Wait for WAL initialization
    usleep(500000);  // 500ms
    
    // Close KVStore
    ppdb_log_info("Closing KVStore...");
    if (store) {
        ppdb_kvstore_close(store);
        store = NULL;  // Prevent use after free
    }
    
    // Wait for resources to be released
    usleep(500000);  // 500ms
    
    // Clean up test directories
    ppdb_log_info("Final cleanup of test directories...");
    cleanup_test_dir(wal_dir);  // Clean up WAL directory first
    cleanup_test_dir(test_dir); // Then clean up parent directory
    
    // Wait for cleanup to complete
    usleep(500000);  // 500ms
    
    ppdb_log_info("Test completed successfully");
    return 0;
}

// 创建测试用的 KVStore
static ppdb_kvstore_t* create_test_kvstore(const char* test_dir, ppdb_mode_t mode) {
    char wal_dir[MAX_PATH_LENGTH];
    snprintf(wal_dir, sizeof(wal_dir), "%s/wal", test_dir);
    
    // 清理测试目录和WAL目录
    cleanup_test_dir(wal_dir);  // 先清理子目录
    cleanup_test_dir(test_dir); // 再清理父目录
    
    // 等待一小段时间确保所有资源都被释放
    usleep(200000);  // 200ms
    
    ppdb_kvstore_config_t config = {
        .dir_path = {0},
        .memtable_size = 4096,
        .mode = mode
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    config.dir_path[sizeof(config.dir_path) - 1] = '\0';  // Ensure null termination
    
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create KVStore: %s", ppdb_error_string(err));
        return NULL;
    }
    if (!store) {
        ppdb_log_error("KVStore pointer is NULL");
        return NULL;
    }

    // 等待初始化完成
    usleep(200000);  // 200ms
    return store;
}

// Test KVStore basic operations
static int test_kvstore_basic_ops(void) {
    ppdb_log_info("Testing KVStore basic operations...");

    // 创建 KVStore
    const char* test_dir = "test_kvstore_basic.db";
    ppdb_kvstore_t* store = create_test_kvstore(test_dir, PPDB_MODE_LOCKED);
    TEST_ASSERT(store != NULL, "Failed to create KVStore");

    // 测试插入和获取
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    ppdb_error_t err = ppdb_kvstore_put(store, (const uint8_t*)test_key, strlen(test_key),
                                       (const uint8_t*)test_value, strlen(test_value));
    TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");

    // 先获取值的大小
    size_t value_size = 0;
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, strlen(test_key),
                          NULL, &value_size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value size");
    TEST_ASSERT(value_size == strlen(test_value), "Value size mismatch");

    // 获取值
    uint8_t* value_buf = NULL;
    size_t actual_size = 0;
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, strlen(test_key),
                          &value_buf, &actual_size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value");
    TEST_ASSERT(actual_size == strlen(test_value), "Value size mismatch");
    TEST_ASSERT(value_buf != NULL, "Value buffer is NULL");
    TEST_ASSERT(memcmp(value_buf, test_value, actual_size) == 0, "Value content mismatch");
    free(value_buf);

    // 测试删除
    err = ppdb_kvstore_delete(store, (const uint8_t*)test_key, strlen(test_key));
    TEST_ASSERT(err == PPDB_OK, "Failed to delete key-value pair");

    // 验证删除
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, strlen(test_key),
                          NULL, &value_size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key still exists after deletion");

    // 关闭 KVStore
    ppdb_kvstore_close(store);

    cleanup_test_dir(test_dir);
    return 0;
}

// Test KVStore recovery
static int test_kvstore_recovery(void) {
    ppdb_log_info("Testing KVStore recovery...");

    const char* test_dir = "test_kvstore_recovery.db";
    cleanup_test_dir(test_dir);

    // 创建第一个KVStore实例并写入数据
    ppdb_kvstore_t* store1 = create_test_kvstore(test_dir, PPDB_MODE_LOCKED);
    TEST_ASSERT(store1 != NULL, "Failed to create first KVStore");

    // 写入一些测试数据
    const char* test_keys[] = {"key1", "key2", "key3"};
    const char* test_values[] = {"value1", "value2", "value3"};
    const int num_pairs = 3;

    for (int i = 0; i < num_pairs; i++) {
        ppdb_error_t err = ppdb_kvstore_put(store1,
                                           (const uint8_t*)test_keys[i], strlen(test_keys[i]),
                                           (const uint8_t*)test_values[i], strlen(test_values[i]));
        TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    }

    // 关闭第一个实例
    ppdb_kvstore_close(store1);

    // 创建第二个KVStore实例并验证数据
    ppdb_kvstore_t* store2 = create_test_kvstore(test_dir, PPDB_MODE_LOCKED);
    TEST_ASSERT(store2 != NULL, "Failed to create second KVStore");

    // 验证所有键值对
    for (int i = 0; i < num_pairs; i++) {
        uint8_t* value = NULL;
        size_t value_size = 0;
        ppdb_error_t err = ppdb_kvstore_get(store2,
                                           (const uint8_t*)test_keys[i], strlen(test_keys[i]),
                                           &value, &value_size);
        TEST_ASSERT(err == PPDB_OK, "Failed to get key-value pair");
        TEST_ASSERT(value_size == strlen(test_values[i]), "Value size mismatch");
        TEST_ASSERT(memcmp(value, test_values[i], value_size) == 0, "Value content mismatch");
        free(value);
    }

    // 关闭第二个实例
    ppdb_kvstore_close(store2);

    cleanup_test_dir(test_dir);
    return 0;
}

// 并发测试参数
#define NUM_THREADS 4
#define NUM_OPS 1000

// 线程参数结构
typedef struct {
    ppdb_kvstore_t* store;
    int thread_id;
    int num_ops;
} thread_args_t;

// 工作线程函数
static void* concurrent_worker(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char key[32];
    char value[32];

    for (int i = 0; i < args->num_ops; i++) {
        // 生成键值对
        snprintf(key, sizeof(key), "key_%d_%d", args->thread_id, i);
        snprintf(value, sizeof(value), "value_%d_%d", args->thread_id, i);

        // 写入键值对
        ppdb_error_t err = ppdb_kvstore_put(args->store,
                                           (const uint8_t*)key, strlen(key),
                                           (const uint8_t*)value, strlen(value));
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d failed to put key-value pair: %s",
                          args->thread_id, ppdb_error_string(err));
            continue;
        }

        // 读取并验证
        uint8_t* read_value = NULL;
        size_t read_size = 0;
        err = ppdb_kvstore_get(args->store,
                              (const uint8_t*)key, strlen(key),
                              &read_value, &read_size);
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d failed to get key-value pair: %s",
                          args->thread_id, ppdb_error_string(err));
            continue;
        }

        // 验证值
        if (read_size != strlen(value) ||
            memcmp(read_value, value, read_size) != 0) {
            ppdb_log_error("Thread %d value mismatch", args->thread_id);
        }
        free(read_value);

        // 删除键值对
        err = ppdb_kvstore_delete(args->store,
                                 (const uint8_t*)key, strlen(key));
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d failed to delete key-value pair: %s",
                          args->thread_id, ppdb_error_string(err));
        }
    }

    return NULL;
}

// 并发操作测试
static int test_kvstore_concurrent_ops(void) {
    ppdb_log_info("Testing KVStore concurrent operations...");

    // 创建 KVStore
    const char* test_dir = "test_kvstore_concurrent.db";
    ppdb_kvstore_t* store = create_test_kvstore(test_dir, PPDB_MODE_LOCKFREE);
    TEST_ASSERT(store != NULL, "Failed to create KVStore");

    // 创建线程
    pthread_t threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].store = store;
        args[i].thread_id = i;
        args[i].num_ops = NUM_OPS;
        int ret = pthread_create(&threads[i], NULL, concurrent_worker, &args[i]);
        TEST_ASSERT(ret == 0, "Failed to create thread");
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // 关闭 KVStore
    ppdb_kvstore_close(store);

    cleanup_test_dir(test_dir);
    return 0;
}

// KVStore test suite
test_suite_t kvstore_suite = {
    .name = "KVStore",
    .cases = (test_case_t[]){
        {"create_close", test_kvstore_create_close},
        {"basic_ops", test_kvstore_basic_ops},
        {"recovery", test_kvstore_recovery},
        {"concurrent", test_kvstore_concurrent_ops}
    },
    .num_cases = 4
};