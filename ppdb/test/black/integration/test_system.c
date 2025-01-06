#include "test_framework.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_internal.h"
#include "internal/base.h"

#define NUM_THREADS 4
#define NUM_OPERATIONS 1000
#define TEST_DIR "/tmp/ppdb_test"

// 测试上下文
typedef struct {
    ppdb_kvstore_t* store;
    int thread_id;
    int success_count;
} test_context_t;

// 完整工作流测试
static int test_full_workflow(void) {
    ppdb_kvstore_t* store = NULL;
    int err;

    // 1. 初始化配置
    ppdb_config_t config = {
        .memtable_size = 1024 * 1024,  // 1MB
        .enable_wal = true,
        .wal_path = TEST_DIR "/wal",
        .sync_write = true,
        .compression_enabled = true
    };

    // 2. 创建存储实例
    err = ppdb_kvstore_create(&store, &config);
    TEST_ASSERT_OK(err, "Failed to create kvstore");
    TEST_ASSERT_NOT_NULL(store, "KVStore is null");

    // 3. 基本操作测试
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    
    // 写入测试
    err = ppdb_kvstore_put(store, test_key, strlen(test_key), 
                          test_value, strlen(test_value));
    TEST_ASSERT_OK(err, "Failed to put key-value");

    // 读取测试
    char read_value[64];
    err = ppdb_kvstore_get(store, test_key, strlen(test_key), 
                          read_value, sizeof(read_value));
    TEST_ASSERT_OK(err, "Failed to get value");
    TEST_ASSERT(strcmp(read_value, test_value) == 0, "Value mismatch");

    // 删除测试
    err = ppdb_kvstore_delete(store, test_key, strlen(test_key));
    TEST_ASSERT_OK(err, "Failed to delete key");

    // 验证删除
    err = ppdb_kvstore_get(store, test_key, strlen(test_key), 
                          read_value, sizeof(read_value));
    TEST_ASSERT(err == PPDB_NOT_FOUND, "Key should not exist after deletion");

    // 4. 批量操作测试
    ppdb_base_thread_t* threads[NUM_THREADS];
    test_context_t contexts[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        contexts[i].store = store;
        contexts[i].thread_id = i;
        contexts[i].success_count = 0;
        err = ppdb_base_thread_create(&threads[i], batch_worker, &contexts[i]);
        TEST_ASSERT(err == 0, "Failed to create thread");
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        ppdb_base_thread_join(threads[i], NULL);
        TEST_ASSERT(contexts[i].success_count > 0, 
                   "Thread %d had no successful operations", i);
    }

    // 5. 强制刷新并重新打开
    err = ppdb_kvstore_sync(store);
    TEST_ASSERT_OK(err, "Failed to sync store");
    ppdb_destroy(store);

    // 重新打开并验证数据
    err = ppdb_kvstore_create(&store, &config);
    TEST_ASSERT_OK(err, "Failed to reopen store");

    // 验证批量操作的数据
    char key[32];
    int found_count = 0;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < NUM_OPERATIONS; i++) {
            snprintf(key, sizeof(key), "key_%d_%d", t, i);
            err = ppdb_kvstore_get(store, key, strlen(key), 
                                 read_value, sizeof(read_value));
            if (err == PPDB_OK) {
                found_count++;
            }
        }
    }

    TEST_ASSERT(found_count > 0, "No data found after reopening");

    // 6. 清理
    ppdb_destroy(store);
    cleanup_test_dir(TEST_DIR);
    
    return 0;
}

// 恢复流程测试
static int test_recovery_workflow(void) {
    ppdb_kvstore_t* store = NULL;
    int err;

    // 1. 配置WAL
    ppdb_config_t config = {
        .enable_wal = true,
        .wal_path = TEST_DIR "/recovery_wal",
        .sync_write = true
    };

    // 2. 第一次运行：写入数据
    err = ppdb_kvstore_create(&store, &config);
    TEST_ASSERT_OK(err, "Failed to create first store");

    // 写入一些数据
    for (int i = 0; i < 100; i++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "recovery_key_%d", i);
        snprintf(value, sizeof(value), "recovery_value_%d", i);
        
        err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
        TEST_ASSERT_OK(err, "Failed to put data for recovery");
    }

    // 3. 模拟崩溃（不调用destroy直接退出）
    store = NULL;

    // 4. 重新打开并恢复
    err = ppdb_kvstore_create(&store, &config);
    TEST_ASSERT_OK(err, "Failed to create store for recovery");

    // 5. 验证数据是否恢复
    char value[32];
    int recovered_count = 0;

    for (int i = 0; i < 100; i++) {
        char key[32], expected[32];
        snprintf(key, sizeof(key), "recovery_key_%d", i);
        snprintf(expected, sizeof(expected), "recovery_value_%d", i);
        
        err = ppdb_kvstore_get(store, key, strlen(key), value, sizeof(value));
        if (err == PPDB_OK && strcmp(value, expected) == 0) {
            recovered_count++;
        }
    }

    TEST_ASSERT(recovered_count == 100, 
               "Recovery incomplete: only %d/100 items recovered", recovered_count);

    // 6. 清理
    ppdb_destroy(store);
    cleanup_test_dir(TEST_DIR);
    
    return 0;
}

// 批量操作工作线程
static void* batch_worker(void* arg) {
    test_context_t* ctx = (test_context_t*)arg;
    char key[32], value[32];
    int err;

    for (int i = 0; i < NUM_OPERATIONS; i++) {
        snprintf(key, sizeof(key), "key_%d_%d", ctx->thread_id, i);
        snprintf(value, sizeof(value), "value_%d_%d", ctx->thread_id, i);
        
        err = ppdb_kvstore_put(ctx->store, key, strlen(key), 
                              value, strlen(value));
        if (err == PPDB_OK) {
            ctx->success_count++;
        }

        // 随机执行一些读取操作
        if (rand() % 4 == 0) {
            int random_key = rand() % i;
            snprintf(key, sizeof(key), "key_%d_%d", ctx->thread_id, random_key);
            err = ppdb_kvstore_get(ctx->store, key, strlen(key), 
                                 value, sizeof(value));
            if (err == PPDB_OK) {
                ctx->success_count++;
            }
        }
    }

    return NULL;
}

// 集成测试套件
static const test_case_t integration_cases[] = {
    {"test_full_workflow", test_full_workflow, 60, false, 
     "Test complete workflow including init, CRUD, batch ops, and restart"},
    {"test_recovery_workflow", test_recovery_workflow, 60, false, 
     "Test WAL recovery after crash"},
    {NULL, NULL, 0, false, NULL}
};

const test_suite_t integration_suite = {
    .name = "Integration Tests",
    .cases = integration_cases,
    .num_cases = sizeof(integration_cases) / sizeof(integration_cases[0]) - 1,
    .setup = NULL,
    .teardown = NULL
};
