#include <cosmopolitan.h>
#include "test_framework.h"
#include <ppdb/kvstore.h>
#include <ppdb/error.h>
#include <ppdb/memtable.h>
#include <ppdb/wal.h>

// 辅助函数：清理测试目录
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
    if (rmdir(dir_path) != 0) {
        ppdb_log_warn("Failed to remove directory: %s, error: %s", dir_path, strerror(errno));
    } else {
        ppdb_log_debug("Removed directory: %s", dir_path);
    }
}

// 创建和关闭测试
static int test_kvstore_create_close(void) {
    ppdb_log_info("Testing KVStore create/close...");
    
    const char* test_dir = "test_create_close.db";
    cleanup_test_dir(test_dir);
    
    // 创建KVStore
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_open(test_dir, &store);
    TEST_ASSERT(err == PPDB_OK, "Failed to create KVStore");
    TEST_ASSERT(store != NULL, "KVStore pointer is NULL");
    
    // 关闭KVStore
    err = ppdb_kvstore_close(store);
    TEST_ASSERT(err == PPDB_OK, "Failed to close KVStore");
    
    cleanup_test_dir(test_dir);
    return 0;
}

// 基本操作测试
static int test_kvstore_basic_ops(void) {
    ppdb_log_info("Testing KVStore basic operations...");
    
    const char* test_dir = "test_basic_ops.db";
    cleanup_test_dir(test_dir);
    
    // 创建KVStore
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_open(test_dir, &store);
    TEST_ASSERT(err == PPDB_OK, "Failed to create KVStore");
    
    // 测试Put操作
    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
    TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    
    // 测试Get操作
    char buf[256] = {0};
    size_t size = 0;
    err = ppdb_kvstore_get(store, key, strlen(key), buf, sizeof(buf), &size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value");
    TEST_ASSERT(strcmp(buf, value) == 0, "Retrieved value does not match");
    
    // 测试Delete操作
    err = ppdb_kvstore_delete(store, key, strlen(key));
    TEST_ASSERT(err == PPDB_OK, "Failed to delete key");
    
    // 验证删除
    err = ppdb_kvstore_get(store, key, strlen(key), buf, sizeof(buf), &size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key still exists after deletion");
    
    // 关闭KVStore
    err = ppdb_kvstore_close(store);
    TEST_ASSERT(err == PPDB_OK, "Failed to close KVStore");
    
    cleanup_test_dir(test_dir);
    return 0;
}

// 恢复测试
static int test_kvstore_recovery(void) {
    ppdb_log_info("Testing KVStore recovery...");
    
    const char* test_dir = "test_recovery.db";
    cleanup_test_dir(test_dir);
    
    // 第一次打开并写入数据
    {
        ppdb_kvstore_t* store = NULL;
        ppdb_error_t err = ppdb_kvstore_open(test_dir, &store);
        TEST_ASSERT(err == PPDB_OK, "Failed to create KVStore");
        
        const char* key = "recovery_key";
        const char* value = "recovery_value";
        err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
        TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
        
        err = ppdb_kvstore_close(store);
        TEST_ASSERT(err == PPDB_OK, "Failed to close KVStore");
    }
    
    // 重新打开并验证数据
    {
        ppdb_kvstore_t* store = NULL;
        ppdb_error_t err = ppdb_kvstore_open(test_dir, &store);
        TEST_ASSERT(err == PPDB_OK, "Failed to reopen KVStore");
        
        const char* key = "recovery_key";
        char buf[256] = {0};
        size_t size = 0;
        err = ppdb_kvstore_get(store, key, strlen(key), buf, sizeof(buf), &size);
        TEST_ASSERT(err == PPDB_OK, "Failed to get value after recovery");
        TEST_ASSERT(strcmp(buf, "recovery_value") == 0, "Recovered value does not match");
        
        err = ppdb_kvstore_close(store);
        TEST_ASSERT(err == PPDB_OK, "Failed to close KVStore");
    }
    
    cleanup_test_dir(test_dir);
    return 0;
}

// 线程数据结构
typedef struct {
    ppdb_kvstore_t* store;
    int thread_id;
    int success_count;
} thread_data_t;

// 并发测试
static int test_kvstore_concurrent(void) {
    ppdb_log_info("Testing KVStore concurrent operations...");
    
    const char* test_dir = "test_concurrent.db";
    cleanup_test_dir(test_dir);
    
    // 创建KVStore
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_open(test_dir, &store);
    TEST_ASSERT(err == PPDB_OK, "Failed to create KVStore");
    
    // 创建多个线程进行并发操作
    #define NUM_THREADS 4
    #define OPS_PER_THREAD 100
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    // 启动线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].store = store;
        thread_data[i].thread_id = i;
        thread_data[i].success_count = 0;
        pthread_create(&threads[i], NULL, concurrent_worker, &thread_data[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        ppdb_log_info("Thread %d completed with %d successful operations", 
                      thread_data[i].thread_id, thread_data[i].success_count);
    }
    
    // 验证所有数据
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            char key[32], expected_value[32];
            snprintf(key, sizeof(key), "key_%d_%d", t, i);
            snprintf(expected_value, sizeof(expected_value), "value_%d_%d", t, i);
            
            char buf[256] = {0};
            size_t size = 0;
            err = ppdb_kvstore_get(store, key, strlen(key), buf, sizeof(buf), &size);
            
            ppdb_log_debug("Final verification: [%s] = [%s] %s", 
                          key, buf, err == PPDB_OK ? "OK" : "Failed");
            
            TEST_ASSERT(err == PPDB_OK, "Failed to get value in verification");
            TEST_ASSERT(strcmp(buf, expected_value) == 0, "Value mismatch in verification");
        }
    }
    
    // 关闭KVStore
    err = ppdb_kvstore_close(store);
    TEST_ASSERT(err == PPDB_OK, "Failed to close KVStore");
    
    cleanup_test_dir(test_dir);
    return 0;
}

// 并发测试的工作线程
static void* concurrent_worker(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    ppdb_kvstore_t* store = data->store;
    int thread_id = data->thread_id;
    
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "key_%d_%d", thread_id, i);
        snprintf(value, sizeof(value), "value_%d_%d", thread_id, i);
        
        // 写入数据
        ppdb_error_t err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
        if (err == PPDB_OK) {
            ppdb_log_debug("Thread %d: Put succeeded [%s] = [%s]", thread_id, key, value);
            data->success_count++;
        } else {
            ppdb_log_error("Thread %d: Put failed [%s] = [%s], error: %d", 
                          thread_id, key, value, err);
            continue;
        }
        
        // 读取并验证数据
        char buf[256] = {0};
        size_t size = 0;
        err = ppdb_kvstore_get(store, key, strlen(key), buf, sizeof(buf), &size);
        if (err == PPDB_OK) {
            ppdb_log_debug("Thread %d: Get succeeded [%s] = [%s]", thread_id, key, buf);
            data->success_count++;
        } else {
            ppdb_log_error("Thread %d: Get failed [%s], error: %d", thread_id, key, err);
        }
    }
    
    return NULL;
}

// KVStore测试套件定义
static const test_case_t kvstore_test_cases[] = {
    {"create_close", test_kvstore_create_close},
    {"basic_ops", test_kvstore_basic_ops},
    {"recovery", test_kvstore_recovery},
    {"concurrent", test_kvstore_concurrent}
};

// 导出KVStore测试套件
const test_suite_t kvstore_suite = {
    .name = "KVStore",
    .cases = kvstore_test_cases,
    .num_cases = sizeof(kvstore_test_cases) / sizeof(kvstore_test_cases[0])
};