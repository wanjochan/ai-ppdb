#include <cosmopolitan.h>
#include <ppdb/kvstore.h>
#include <ppdb/memtable.h>
#include <ppdb/wal.h>
#include <ppdb/defs.h>
#include "test_framework.h"

#undef MAX_PATH_LENGTH
#define MAX_PATH_LENGTH 256

#define NUM_THREADS 4
#define OPS_PER_THREAD 100

// 线程数据结构
typedef struct {
    ppdb_kvstore_t* store;
    int thread_id;
    int num_operations;
    int success_count;
} thread_data_t;

// 函数声明
static int test_kvstore_create_close(void);
static int test_kvstore_null_params(void);
static int test_kvstore_basic_ops(void);
static int test_kvstore_recovery(void);
static int test_kvstore_concurrent(void);
static void* concurrent_worker(void* arg);
static void cleanup_test_dir(const char* dir_path);

// 测试用例定义
static const test_case_t kvstore_test_cases[] = {
    {"create_close", test_kvstore_create_close},
    {"null_params", test_kvstore_null_params},
    {"basic_ops", test_kvstore_basic_ops},
    {"recovery", test_kvstore_recovery},
    {"concurrent", test_kvstore_concurrent}
};

// 获取测试用例数量
size_t get_kvstore_test_case_count(void) {
    return sizeof(kvstore_test_cases) / sizeof(test_case_t);
}

// 获取测试用例数组
const test_case_t* get_kvstore_test_cases(void) {
    return kvstore_test_cases;
}

// 测试套件定义
const test_suite_t kvstore_suite = {
    .name = "KVStore",
    .cases = kvstore_test_cases,
    .num_cases = sizeof(kvstore_test_cases) / sizeof(kvstore_test_cases[0])
};

// 清理测试目录
static void cleanup_test_dir(const char* dir_path) {
    ppdb_log_debug("Cleaning up test directory: %s", dir_path);

    // 如果目录不存在，直接返回
    struct stat st;
    if (stat(dir_path, &st) != 0) {
        ppdb_log_debug("Directory does not exist: %s", dir_path);
        return;
    }

    // 如果不是目录，尝试直接删除文件
    if (!S_ISDIR(st.st_mode)) {
        if (unlink(dir_path) != 0) {
            ppdb_log_warn("Failed to delete file: %s, error: %s", dir_path, strerror(errno));
        } else {
            ppdb_log_debug("Deleted file: %s", dir_path);
        }
        return;
    }

    DIR* dir = opendir(dir_path);
    if (!dir) {
        ppdb_log_warn("Could not open directory for cleanup: %s, error: %s", dir_path, strerror(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[MAX_PATH_LENGTH];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        // 递归删除子目录
        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                cleanup_test_dir(path);
            } else {
                if (unlink(path) != 0) {
                    ppdb_log_warn("Failed to delete file: %s, error: %s", path, strerror(errno));
                } else {
                    ppdb_log_debug("Deleted file: %s", path);
                }
            }
        }
    }

    closedir(dir);

    // 删除空目录
    if (rmdir(dir_path) != 0) {
        ppdb_log_warn("Failed to remove directory: %s, error: %s", dir_path, strerror(errno));
    } else {
        ppdb_log_debug("Removed directory: %s", dir_path);
    }
}

// 基本的创建和关闭测试
static int test_kvstore_create_close(void) {
    ppdb_log_info("Running test: test_kvstore_create_close");
    ppdb_log_info("Testing KVStore create/close...");

    const char* test_path = "test_create.db";
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err;
    int result = 0;

    // 清理测试目录
    cleanup_test_dir(test_path);
    char wal_path[MAX_PATH_LENGTH];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", test_path);
    cleanup_test_dir(wal_path);

    // 创建 KVStore
    err = ppdb_kvstore_open(test_path, &store);
    TEST_ASSERT_OK(err, "Create KVStore");
    TEST_ASSERT_NOT_NULL(store, "KVStore instance should not be NULL");

    // 关闭 KVStore
    ppdb_kvstore_close(store);

    // 清理测试目录
    cleanup_test_dir(test_path);
    cleanup_test_dir(wal_path);

    ppdb_log_info("KVStore create/close test passed");
    return result;
}

// 参数验证测试
static int test_kvstore_null_params(void) {
    ppdb_log_info("Running test: test_kvstore_null_params");
    ppdb_log_info("Testing KVStore parameter validation...");

    const char* test_path = "test.db";
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err;
    int result = 0;

    // 清理测试目录
    cleanup_test_dir(test_path);
    char wal_path[MAX_PATH_LENGTH];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", test_path);
    cleanup_test_dir(wal_path);

    // 测试空路径
    err = ppdb_kvstore_open(NULL, &store);
    TEST_ASSERT(err == PPDB_ERR_INVALID_ARG, "Open with NULL path should fail");

    // 测试空存储指针
    err = ppdb_kvstore_open(test_path, NULL);
    TEST_ASSERT(err == PPDB_ERR_INVALID_ARG, "Open with NULL store pointer should fail");

    ppdb_log_info("KVStore parameter validation test passed");
    return result;
}

// 基本操作测试
static int test_kvstore_basic_ops(void) {
    ppdb_log_info("Running test: test_kvstore_basic_ops");
    ppdb_log_info("Testing KVStore basic operations...");

    const char* test_path = "test_basic.db";
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err;
    int result = 0;

    // 清理测试目录
    cleanup_test_dir(test_path);
    char wal_path[MAX_PATH_LENGTH];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", test_path);
    cleanup_test_dir(wal_path);

    // 创建 KVStore
    err = ppdb_kvstore_open(test_path, &store);
    TEST_ASSERT_OK(err, "Create KVStore");
    TEST_ASSERT_NOT_NULL(store, "KVStore instance should not be NULL");

    // 写入数据
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    err = ppdb_kvstore_put(store, (const uint8_t*)test_key, strlen(test_key) + 1,
                          (const uint8_t*)test_value, strlen(test_value) + 1);
    TEST_ASSERT_OK(err, "Put data");

    // 读取数据
    uint8_t read_buf[256];
    size_t read_size = sizeof(read_buf);
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, strlen(test_key) + 1,
                          read_buf, &read_size);
    TEST_ASSERT_OK(err, "Get data");
    TEST_ASSERT_STR_EQ((const char*)read_buf, test_value, "Read value should match written value");

    // 删除数据
    err = ppdb_kvstore_delete(store, (const uint8_t*)test_key, strlen(test_key) + 1);
    TEST_ASSERT_OK(err, "Delete data");

    // 验证删除
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, strlen(test_key) + 1,
                          read_buf, &read_size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Get deleted data should fail");

    // 关闭 KVStore
    ppdb_kvstore_close(store);

    // 清理测试目录
    cleanup_test_dir(test_path);
    cleanup_test_dir(wal_path);

    ppdb_log_info("KVStore basic operations test passed");
    return result;
}

// 恢复测试
static int test_kvstore_recovery(void) {
    ppdb_log_info("Running test: test_kvstore_recovery");
    ppdb_log_info("Testing KVStore recovery...");

    const char* test_path = "test_recovery.db";
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err;
    int result = 0;

    // 清理测试目录
    cleanup_test_dir(test_path);
    char wal_path[MAX_PATH_LENGTH];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", test_path);
    cleanup_test_dir(wal_path);

    // 第一阶段：写入多条数据
    err = ppdb_kvstore_open(test_path, &store);
    TEST_ASSERT_OK(err, "Open KVStore for recovery test");
    TEST_ASSERT_NOT_NULL(store, "KVStore instance should not be NULL");

    // 写入多个键值对
    const char* test_keys[] = {"recovery_key1", "recovery_key2", "recovery_key3"};
    const char* test_values[] = {"recovery_value1", "recovery_value2", "recovery_value3"};
    const int num_pairs = 3;

    for (int i = 0; i < num_pairs; i++) {
        err = ppdb_kvstore_put(store, (const uint8_t*)test_keys[i], strlen(test_keys[i]) + 1,
                              (const uint8_t*)test_values[i], strlen(test_values[i]) + 1);
        TEST_ASSERT_OK(err, "Put data for recovery test");
        ppdb_log_info("Written key-value pair %d: [%s] = [%s]", i, test_keys[i], test_values[i]);
    }

    // 删除一个键值对
    err = ppdb_kvstore_delete(store, (const uint8_t*)test_keys[1], strlen(test_keys[1]) + 1);
    TEST_ASSERT_OK(err, "Delete data for recovery test");
    ppdb_log_info("Deleted key: [%s]", test_keys[1]);

    ppdb_kvstore_close(store);
    ppdb_log_info("First phase: data written and deleted successfully");

    // 第二阶段：重新打开并验证数据
    err = ppdb_kvstore_open(test_path, &store);
    TEST_ASSERT_OK(err, "Reopen KVStore for recovery test");
    TEST_ASSERT_NOT_NULL(store, "KVStore instance should not be NULL after reopen");

    // 验证所有键值对
    for (int i = 0; i < num_pairs; i++) {
        uint8_t read_buf[256];
        size_t read_size = sizeof(read_buf);
        err = ppdb_kvstore_get(store, (const uint8_t*)test_keys[i], strlen(test_keys[i]) + 1,
                              read_buf, &read_size);
        
        if (i == 1) {
            // 验证已删除的键
            TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Deleted key should not be found");
            ppdb_log_info("Verified deleted key [%s] is not found", test_keys[i]);
        } else {
            // 验证其他键
            TEST_ASSERT_OK(err, "Get recovered data");
            TEST_ASSERT_STR_EQ((const char*)read_buf, test_values[i], "Read value should match written value");
            ppdb_log_info("Verified key-value pair: [%s] = [%s]", test_keys[i], (const char*)read_buf);
        }
    }

    // 第三阶段：写入新数据并再次验证
    const char* new_key = "new_recovery_key";
    const char* new_value = "new_recovery_value";
    err = ppdb_kvstore_put(store, (const uint8_t*)new_key, strlen(new_key) + 1,
                          (const uint8_t*)new_value, strlen(new_value) + 1);
    TEST_ASSERT_OK(err, "Put new data after recovery");
    ppdb_log_info("Written new key-value pair: [%s] = [%s]", new_key, new_value);

    ppdb_kvstore_close(store);

    // 第四阶段：最终验证
    err = ppdb_kvstore_open(test_path, &store);
    TEST_ASSERT_OK(err, "Final reopen of KVStore");
    TEST_ASSERT_NOT_NULL(store, "KVStore instance should not be NULL after final reopen");

    // 验证新写入的数据
    uint8_t final_buf[256];
    size_t final_size = sizeof(final_buf);
    err = ppdb_kvstore_get(store, (const uint8_t*)new_key, strlen(new_key) + 1,
                          final_buf, &final_size);
    TEST_ASSERT_OK(err, "Get new data after final recovery");
    TEST_ASSERT_STR_EQ((const char*)final_buf, new_value, "Final read value should match written value");
    ppdb_log_info("Verified new key-value pair after final recovery: [%s] = [%s]", 
                  new_key, (const char*)final_buf);

    ppdb_kvstore_close(store);

    // 清理测试目录
    cleanup_test_dir(test_path);
    cleanup_test_dir(wal_path);

    ppdb_log_info("KVStore recovery test completed successfully");
    return result;
}

// 并发测试
static int test_kvstore_concurrent(void) {
    ppdb_log_info("Running test: test_kvstore_concurrent");
    ppdb_log_info("Testing KVStore concurrent operations...");

    const char* test_path = "test_concurrent.db";
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err;
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    int total_success = 0;
    int result = 0;

    // 清理测试目录
    cleanup_test_dir(test_path);
    char wal_path[MAX_PATH_LENGTH];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", test_path);
    cleanup_test_dir(wal_path);

    // 创建 KVStore
    err = ppdb_kvstore_open(test_path, &store);
    TEST_ASSERT_OK(err, "Open KVStore for concurrent test");
    TEST_ASSERT_NOT_NULL(store, "KVStore instance should not be NULL");

    // 创建并启动线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].store = store;
        thread_data[i].thread_id = i;
        thread_data[i].num_operations = OPS_PER_THREAD;
        thread_data[i].success_count = 0;

        ppdb_log_info("Created thread %d", i);
        err = pthread_create(&threads[i], NULL, concurrent_worker, &thread_data[i]);
        TEST_ASSERT_OK(err, "Create worker thread");
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        total_success += thread_data[i].success_count;
    }

    // 验证结果
    ppdb_log_info("Total successful operations: %d", total_success);
    TEST_ASSERT(total_success == NUM_THREADS * OPS_PER_THREAD * 2, 
                "All operations should succeed");

    // 关闭 KVStore
    ppdb_kvstore_close(store);

    // 清理测试目录
    cleanup_test_dir(test_path);
    cleanup_test_dir(wal_path);

    ppdb_log_info("KVStore concurrent test completed successfully");
    return result;
}

// 并发测试线程函数
static void* concurrent_worker(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    ppdb_kvstore_t* store = data->store;
    int thread_id = data->thread_id;
    int num_operations = data->num_operations;
    data->success_count = 0;

    char key[32];
    char value[32];
    uint8_t read_buf[256];
    size_t read_size;
    ppdb_error_t err;

    for (int i = 0; i < num_operations; i++) {
        // 构造唯一的键值对
        snprintf(key, sizeof(key), "key_%d_%d", thread_id, i);
        snprintf(value, sizeof(value), "value_%d_%d", thread_id, i);

        // 写入操作
        err = ppdb_kvstore_put(store, (const uint8_t*)key, strlen(key) + 1, 
                              (const uint8_t*)value, strlen(value) + 1);
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d: Put failed [%s] = [%s]", thread_id, key, value);
            continue;
        }
        ppdb_log_info("Thread %d: Put succeeded [%s] = [%s]", thread_id, key, value);
        data->success_count++;

        // 读取操作
        read_size = sizeof(read_buf);
        err = ppdb_kvstore_get(store, (const uint8_t*)key, strlen(key) + 1, 
                              read_buf, &read_size);
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d: Get failed [%s]", thread_id, key);
            continue;
        }

        // 验证读取的值
        if (strcmp((const char*)read_buf, value) != 0) {
            ppdb_log_error("Thread %d: Value mismatch [%s]: expected [%s], got [%s]",
                          thread_id, key, value, (const char*)read_buf);
            continue;
        }

        ppdb_log_info("Thread %d: Get succeeded [%s] = [%s]", thread_id, key, (const char*)read_buf);
        data->success_count++;
    }

    return NULL;
}