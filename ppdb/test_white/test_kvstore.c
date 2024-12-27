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
    ppdb_log_info("=== Starting test: test_kvstore_create_close ===");
    ppdb_log_info("Testing KVStore create/close...");

    const char* test_path = "test_create.db";
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err;
    int result = 0;

    // 清理测试目录
    ppdb_log_info("Cleaning up test directories...");
    cleanup_test_dir(test_path);
    char wal_path[MAX_PATH_LENGTH];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", test_path);
    cleanup_test_dir(wal_path);

    // 创建 KVStore
    ppdb_log_info("Creating KVStore at: %s", test_path);
    err = ppdb_kvstore_open(test_path, &store);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create KVStore: %d", err);
        result = 1;
        goto cleanup;
    }
    if (!store) {
        ppdb_log_error("KVStore instance is NULL");
        result = 1;
        goto cleanup;
    }
    ppdb_log_info("Successfully created KVStore");

    // 关闭 KVStore
    ppdb_log_info("Closing KVStore...");
    ppdb_kvstore_close(store);
    ppdb_log_info("Successfully closed KVStore");

cleanup:
    // 清理测试目录
    ppdb_log_info("Final cleanup of test directories...");
    cleanup_test_dir(test_path);
    cleanup_test_dir(wal_path);

    if (result == 0) {
        ppdb_log_info("=== Test test_kvstore_create_close passed ===");
    } else {
        ppdb_log_error("=== Test test_kvstore_create_close failed ===");
    }
    return result;
}

// 参数验证测试
static int test_kvstore_null_params(void) {
    ppdb_log_info("=== Starting test: test_kvstore_null_params ===");
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
    if (err != PPDB_ERR_INVALID_ARG) {
        ppdb_log_error("Open with NULL path should fail with INVALID_ARG, got: %d", err);
        result = 1;
        goto cleanup;
    }

    // 测试空存储指针
    err = ppdb_kvstore_open(test_path, NULL);
    if (err != PPDB_ERR_INVALID_ARG) {
        ppdb_log_error("Open with NULL store pointer should fail with INVALID_ARG, got: %d", err);
        result = 1;
        goto cleanup;
    }

cleanup:
    if (result == 0) {
        ppdb_log_info("=== Test test_kvstore_null_params passed ===");
    } else {
        ppdb_log_error("=== Test test_kvstore_null_params failed ===");
    }
    return result;
}

// 基本操作测试
static int test_kvstore_basic_ops(void) {
    ppdb_log_info("=== Starting test: test_kvstore_basic_ops ===");
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
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create KVStore: %d", err);
        result = 1;
        goto cleanup;
    }
    if (!store) {
        ppdb_log_error("KVStore instance is NULL");
        result = 1;
        goto cleanup;
    }

    // 写入数据
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    err = ppdb_kvstore_put(store, (const uint8_t*)test_key, strlen(test_key) + 1,
                          (const uint8_t*)test_value, strlen(test_value) + 1);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to put data: %d", err);
        result = 1;
        goto cleanup;
    }

    // 读取数据
    uint8_t read_buf[256];
    size_t read_size = sizeof(read_buf);
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, strlen(test_key) + 1,
                          read_buf, &read_size);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to get data: %d", err);
        result = 1;
        goto cleanup;
    }
    if (strcmp((const char*)read_buf, test_value) != 0) {
        ppdb_log_error("Value mismatch: expected '%s', got '%s'", test_value, (const char*)read_buf);
        result = 1;
        goto cleanup;
    }

    // 删除数据
    err = ppdb_kvstore_delete(store, (const uint8_t*)test_key, strlen(test_key) + 1);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to delete data: %d", err);
        result = 1;
        goto cleanup;
    }

    // 验证删除
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, strlen(test_key) + 1,
                          read_buf, &read_size);
    if (err != PPDB_ERR_NOT_FOUND) {
        ppdb_log_error("Get deleted data should fail with NOT_FOUND, got: %d", err);
        result = 1;
        goto cleanup;
    }

cleanup:
    // 关闭 KVStore
    if (store) {
        ppdb_kvstore_close(store);
    }

    // 清理测试目录
    cleanup_test_dir(test_path);
    cleanup_test_dir(wal_path);

    if (result == 0) {
        ppdb_log_info("=== Test test_kvstore_basic_ops passed ===");
    } else {
        ppdb_log_error("=== Test test_kvstore_basic_ops failed ===");
    }
    return result;
}

// 恢复测试
static int test_kvstore_recovery(void) {
    ppdb_log_info("=== Starting test: test_kvstore_recovery ===");
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
    ppdb_log_info("Phase 1: Writing initial data...");
    err = ppdb_kvstore_open(test_path, &store);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create KVStore: %d", err);
        result = 1;
        goto cleanup;
    }
    if (!store) {
        ppdb_log_error("KVStore instance is NULL");
        result = 1;
        goto cleanup;
    }

    // 写入测试数据
    const char* test_keys[] = {"key1", "key2", "key3"};
    const char* test_values[] = {"value1", "value2", "value3"};
    const int num_records = sizeof(test_keys) / sizeof(test_keys[0]);

    for (int i = 0; i < num_records; i++) {
        err = ppdb_kvstore_put(store, (const uint8_t*)test_keys[i], strlen(test_keys[i]) + 1,
                              (const uint8_t*)test_values[i], strlen(test_values[i]) + 1);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to put data [%s] = [%s]: %d", test_keys[i], test_values[i], err);
            result = 1;
            goto cleanup;
        }
        ppdb_log_info("Written [%s] = [%s]", test_keys[i], test_values[i]);
    }

    // 关闭 KVStore
    ppdb_kvstore_close(store);
    store = NULL;

    // 第二阶段：重新打开并验证数据
    ppdb_log_info("Phase 2: Reopening KVStore and verifying data...");
    err = ppdb_kvstore_open(test_path, &store);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to reopen KVStore: %d", err);
        result = 1;
        goto cleanup;
    }
    if (!store) {
        ppdb_log_error("Reopened KVStore instance is NULL");
        result = 1;
        goto cleanup;
    }

    // 验证数据
    uint8_t read_buf[256];
    size_t read_size;
    for (int i = 0; i < num_records; i++) {
        read_size = sizeof(read_buf);
        err = ppdb_kvstore_get(store, (const uint8_t*)test_keys[i], strlen(test_keys[i]) + 1,
                              read_buf, &read_size);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to get data [%s]: %d", test_keys[i], err);
            result = 1;
            goto cleanup;
        }
        if (strcmp((const char*)read_buf, test_values[i]) != 0) {
            ppdb_log_error("Value mismatch for key [%s]: expected [%s], got [%s]",
                          test_keys[i], test_values[i], (const char*)read_buf);
            result = 1;
            goto cleanup;
        }
        ppdb_log_info("Verified [%s] = [%s]", test_keys[i], test_values[i]);
    }

cleanup:
    // 关闭 KVStore
    if (store) {
        ppdb_kvstore_close(store);
    }

    // 清理测试目录
    cleanup_test_dir(test_path);
    cleanup_test_dir(wal_path);

    if (result == 0) {
        ppdb_log_info("=== Test test_kvstore_recovery passed ===");
    } else {
        ppdb_log_error("=== Test test_kvstore_recovery failed ===");
    }
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
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create KVStore: %d", err);
        result = 1;
        goto cleanup;
    }
    if (!store) {
        ppdb_log_error("KVStore instance is NULL");
        result = 1;
        goto cleanup;
    }

    ppdb_log_info("Starting concurrent test with %d threads, %d operations per thread", 
                  NUM_THREADS, OPS_PER_THREAD);

    // 创建并启动线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].store = store;
        thread_data[i].thread_id = i;
        thread_data[i].num_operations = OPS_PER_THREAD;
        thread_data[i].success_count = 0;

        ppdb_log_info("Creating thread %d", i);
        err = pthread_create(&threads[i], NULL, concurrent_worker, &thread_data[i]);
        if (err != 0) {
            ppdb_log_error("Failed to create thread %d: %s", i, strerror(err));
            result = 1;
            goto cleanup;
        }
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        void* thread_result;
        err = pthread_join(threads[i], &thread_result);
        if (err != 0) {
            ppdb_log_error("Failed to join thread %d: %s", i, strerror(err));
            result = 1;
            goto cleanup;
        }
        if (thread_result != NULL) {
            ppdb_log_error("Thread %d failed with error", i);
            result = 1;
            goto cleanup;
        }
        total_success += thread_data[i].success_count;
        ppdb_log_info("Thread %d completed with %d successful operations", 
                     i, thread_data[i].success_count);
    }

    // 验证结果
    int expected_success = NUM_THREADS * OPS_PER_THREAD * 2;
    ppdb_log_info("Total successful operations: %d (expected: %d)", 
                  total_success, expected_success);
    
    if (total_success != expected_success) {
        ppdb_log_error("Operation count mismatch: got %d, expected %d", 
                      total_success, expected_success);
        result = 1;
        goto cleanup;
    }

    // 验证所有数据
    uint8_t read_buf[256];
    size_t read_size;
    char key[32];
    char value[32];

    for (int i = 0; i < NUM_THREADS; i++) {
        for (int j = 0; j < OPS_PER_THREAD; j++) {
            snprintf(key, sizeof(key), "key_%d_%d", i, j);
            snprintf(value, sizeof(value), "value_%d_%d", i, j);
            
            read_size = sizeof(read_buf);
            err = ppdb_kvstore_get(store, (const uint8_t*)key, strlen(key) + 1, 
                                 read_buf, &read_size);
            if (err != PPDB_OK) {
                ppdb_log_error("Final verification failed: Get [%s] failed with error %d", 
                              key, err);
                result = 1;
                goto cleanup;
            }
            if (strcmp((const char*)read_buf, value) != 0) {
                ppdb_log_error("Final verification failed: Value mismatch for [%s]: expected [%s], got [%s]",
                              key, value, (const char*)read_buf);
                result = 1;
                goto cleanup;
            }
            ppdb_log_debug("Final verification: [%s] = [%s] OK", key, value);
        }
    }

cleanup:
    // 关闭 KVStore
    if (store) {
        ppdb_log_info("Closing KVStore at: %s", test_path);
        ppdb_kvstore_close(store);
    }

    // 清理测试目录
    ppdb_log_info("Cleaning up test directory: %s", test_path);
    cleanup_test_dir(test_path);
    ppdb_log_info("Cleaning up test directory: %s", wal_path);
    cleanup_test_dir(wal_path);

    if (result == 0) {
        ppdb_log_info("KVStore concurrent test completed successfully");
    } else {
        ppdb_log_error("KVStore concurrent test failed");
    }
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
            ppdb_log_error("Thread %d: Put failed [%s] = [%s], error: %d", 
                          thread_id, key, value, err);
            return (void*)1;
        }
        ppdb_log_info("Thread %d: Put succeeded [%s] = [%s]", thread_id, key, value);
        data->success_count++;

        // 读取操作
        read_size = sizeof(read_buf);
        err = ppdb_kvstore_get(store, (const uint8_t*)key, strlen(key) + 1, 
                              read_buf, &read_size);
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d: Get failed [%s], error: %d", 
                          thread_id, key, err);
            return (void*)1;
        }

        // 验证读取的值
        if (strcmp((const char*)read_buf, value) != 0) {
            ppdb_log_error("Thread %d: Value mismatch [%s]: expected [%s], got [%s]",
                          thread_id, key, value, (const char*)read_buf);
            return (void*)1;
        }

        ppdb_log_info("Thread %d: Get succeeded [%s] = [%s]", 
                     thread_id, key, (const char*)read_buf);
        data->success_count++;
    }

    ppdb_log_info("Thread %d completed all operations with %d successes", 
                  thread_id, data->success_count);
    return NULL;
}