#include <cosmopolitan.h>
#include "ppdb/logger.h"
#include "src/kvstore/kvstore.h"

// 测试KVStore创建和关闭
static bool test_create_close() {
    printf("Testing KVStore create/close...\n");
    
    // 创建KVStore
    const char* path = "test_kvstore_create.db";
    kvstore_t* store = kvstore_create(path);
    if (!store) {
        printf("Failed to create KVStore at: %s\n", path);
        return false;
    }
    
    // 销毁KVStore
    kvstore_destroy(store);
    return true;
}

// 测试KVStore基本操作
static bool test_basic_ops() {
    printf("Testing KVStore basic operations...\n");
    
    // 创建KVStore
    const char* path = "test_kvstore_basic.db";
    kvstore_t* store = kvstore_create(path);
    if (!store) {
        printf("Failed to create KVStore at: %s\n", path);
        return false;
    }
    
    // 插入键值对
    const char* key = "test_key";
    const char* value = "test_value";
    if (!kvstore_put(store, key, strlen(key), value, strlen(value) + 1)) {
        printf("Failed to put key-value pair\n");
        kvstore_destroy(store);
        return false;
    }
    
    // 查找键值对
    void* found_value;
    uint32_t found_len;
    if (!kvstore_get(store, key, strlen(key), &found_value, &found_len)) {
        printf("Failed to get key-value pair\n");
        kvstore_destroy(store);
        return false;
    }
    
    // 验证值
    if (found_len != strlen(value) + 1 || strcmp(found_value, value) != 0) {
        printf("Value mismatch\n");
        free(found_value);
        kvstore_destroy(store);
        return false;
    }
    free(found_value);
    
    // 删除键值对
    if (!kvstore_delete(store, key, strlen(key))) {
        printf("Failed to delete key-value pair\n");
        kvstore_destroy(store);
        return false;
    }
    
    // 验证键值对已删除
    if (kvstore_get(store, key, strlen(key), &found_value, &found_len)) {
        printf("Key still exists after deletion\n");
        free(found_value);
        kvstore_destroy(store);
        return false;
    }
    
    // 销毁KVStore
    kvstore_destroy(store);
    return true;
}

// 测试KVStore恢复
static bool test_recovery() {
    printf("Testing KVStore recovery...\n");
    
    // 创建KVStore并写入数据
    const char* path = "test_kvstore_recovery.db";
    kvstore_t* store = kvstore_create(path);
    if (!store) {
        printf("Failed to create KVStore at: %s\n", path);
        return false;
    }
    
    const char* key = "recovery_key";
    const char* value = "recovery_value";
    if (!kvstore_put(store, key, strlen(key), value, strlen(value) + 1)) {
        printf("Failed to put key-value pair\n");
        kvstore_destroy(store);
        return false;
    }
    
    // 关闭KVStore
    kvstore_close(store);
    
    // 重新打开KVStore
    store = kvstore_create(path);
    if (!store) {
        printf("Failed to reopen KVStore at: %s\n", path);
        return false;
    }
    
    // 验证数据恢复
    void* found_value;
    uint32_t found_len;
    if (!kvstore_get(store, key, strlen(key), &found_value, &found_len)) {
        printf("Failed to get recovered key-value pair\n");
        kvstore_destroy(store);
        return false;
    }
    
    // 验证值
    if (found_len != strlen(value) + 1 || strcmp(found_value, value) != 0) {
        printf("Recovered value mismatch\n");
        free(found_value);
        kvstore_destroy(store);
        return false;
    }
    free(found_value);
    
    // 销毁KVStore
    kvstore_destroy(store);
    return true;
}

// 并发测试线程函数
static void* concurrent_test_thread(void* arg) {
    kvstore_t* store = (kvstore_t*)arg;
    char key[32];
    char value[32];
    
    for (uint32_t i = 0; i < 100; i++) {
        // 生成唯一的键值对
        sprintf(key, "key%u-%lu", i, (unsigned long)pthread_self());
        sprintf(value, "value%u-%lu", i, (unsigned long)pthread_self());
        
        // 插入键值对
        if (!kvstore_put(store, key, strlen(key), value, strlen(value) + 1)) {
            printf("Thread %lu: Failed to put key-value pair\n", (unsigned long)pthread_self());
            continue;
        }
        
        // 查找键值对
        void* found_value;
        uint32_t found_len;
        if (!kvstore_get(store, key, strlen(key), &found_value, &found_len)) {
            printf("Thread %lu: Failed to get key-value pair\n", (unsigned long)pthread_self());
            continue;
        }
        free(found_value);
    }
    
    return NULL;
}

// 测试KVStore并发操作
static bool test_concurrent() {
    printf("Testing KVStore concurrent operations...\n");
    
    // 创建KVStore
    const char* path = "test_kvstore_concurrent.db";
    kvstore_t* store = kvstore_create(path);
    if (!store) {
        printf("Failed to create KVStore at: %s\n", path);
        return false;
    }
    
    // 创建线程
    pthread_t threads[4];
    for (uint32_t i = 0; i < 4; i++) {
        if (pthread_create(&threads[i], NULL, concurrent_test_thread, store) != 0) {
            printf("Failed to create thread %u\n", i);
            kvstore_destroy(store);
            return false;
        }
    }
    
    // 等待线程完成
    for (uint32_t i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 验证总操作数
    printf("Total successful operations: %u\n", kvstore_size(store));
    
    // 销毁KVStore
    kvstore_destroy(store);
    return true;
}

// KVStore测试用例
static test_case_t kvstore_test_cases[] = {
    {"create_close", test_create_close},
    {"basic_ops", test_basic_ops},
    {"recovery", test_recovery},
    {"concurrent", test_concurrent}
};

// KVStore测试套件
static test_suite_t kvstore_test_suite = {
    "KVStore",
    kvstore_test_cases,
    sizeof(kvstore_test_cases) / sizeof(kvstore_test_cases[0])
};

int main() {
    printf("Starting KVStore tests...\n");
    
    // 初始化日志系统
    ppdb_log_init(NULL);
    
    // 运行测试套件
    int result = run_test_suite(&kvstore_test_suite);
    
    printf("KVStore tests completed with result: %d\n", result);
    return result;
} 