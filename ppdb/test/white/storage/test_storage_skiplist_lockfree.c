#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"
#include "../test_framework.h"

// 全局测试数据
static ppdb_base_t* g_base = NULL;

// 测试初始化
static int test_setup(void) {
    printf("\n=== Setting up skiplist test environment ===\n");
    
    // 初始化 base 配置
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024 * 10,  // 10MB
        .thread_pool_size = 4,
        .thread_safe = true,
        .enable_logging = true,
        .log_level = PPDB_LOG_DEBUG
    };
    
    // 初始化 base 层
    ASSERT_OK(ppdb_base_init(&g_base, &base_config));
    
    printf("Test environment setup completed\n");
    return 0;
}

// 测试清理
static int test_teardown(void) {
    printf("\n=== Cleaning up skiplist test environment ===\n");
    
    if (g_base) {
        ppdb_base_destroy(g_base);
        g_base = NULL;
    }
    
    printf("Test environment cleanup completed\n");
    return 0;
}

// 创建测试用的key和value
static void create_test_kv(const char* key_str, const char* value_str, 
                          ppdb_key_t* key, ppdb_value_t* value) {
    key->size = strlen(key_str);
    key->data = ppdb_base_malloc(key->size);
    memcpy(key->data, key_str, key->size);
    
    value->size = strlen(value_str);
    value->data = ppdb_base_malloc(value->size);
    memcpy(value->data, value_str, value->size);
}

// 释放key和value
static void free_test_kv(ppdb_key_t* key, ppdb_value_t* value) {
    if (key && key->data) {
        ppdb_base_free(key->data);
        key->data = NULL;
    }
    if (value && value->data) {
        ppdb_base_free(value->data);
        value->data = NULL;
    }
}

// 基础功能测试
static int test_skiplist_basic(void) {
    printf("\n=== Running basic skiplist tests ===\n");
    
    // 创建跳表
    ppdb_skiplist_t* list = NULL;
    ASSERT_OK(ppdb_skiplist_create(&list));
    
    // 准备测试数据
    ppdb_key_t key1;
    ppdb_value_t value1;
    create_test_kv("key1", "value1", &key1, &value1);
    
    // 测试插入
    ASSERT_OK(ppdb_skiplist_insert(list, &key1, &value1));
    
    // 测试查找
    ppdb_value_t found_value;
    ASSERT_OK(ppdb_skiplist_find(list, &key1, &found_value));
    ASSERT_EQ(found_value.size, value1.size);
    ASSERT_EQ(memcmp(found_value.data, value1.data, value1.size), 0);
    
    // 测试删除
    ASSERT_OK(ppdb_skiplist_remove(list, &key1));
    ASSERT_ERR(ppdb_skiplist_find(list, &key1, &found_value), PPDB_ERR_NOT_FOUND);
    
    // 清理
    free_test_kv(&key1, &value1);
    ppdb_skiplist_destroy(list);
    printf("Basic skiplist tests completed\n");
    return 0;
}

// 并发操作测试
typedef struct {
    ppdb_skiplist_t* list;
    int thread_id;
} thread_data_t;

static void* concurrent_insert_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    ppdb_skiplist_t* list = data->list;
    int thread_id = data->thread_id;
    
    for (int i = 0; i < 100; i++) {
        char key_str[32], value_str[32];
        snprintf(key_str, sizeof(key_str), "key_%d_%d", thread_id, i);
        snprintf(value_str, sizeof(value_str), "value_%d", i);
        
        ppdb_key_t key;
        ppdb_value_t value;
        create_test_kv(key_str, value_str, &key, &value);
        
        ppdb_skiplist_insert(list, &key, &value);
        
        free_test_kv(&key, &value);
    }
    
    return NULL;
}

static int test_skiplist_concurrent(void) {
    printf("\n=== Running concurrent skiplist tests ===\n");
    
    // 创建跳表
    ppdb_skiplist_t* list = NULL;
    ASSERT_OK(ppdb_skiplist_create(&list));
    
    // 创建多个线程进行并发插入
    ppdb_base_thread_t threads[4];
    thread_data_t thread_data[4];
    
    for (int i = 0; i < 4; i++) {
        thread_data[i].list = list;
        thread_data[i].thread_id = i;
        ASSERT_OK(ppdb_base_thread_create(&threads[i], concurrent_insert_thread, &thread_data[i]));
    }
    
    // 等待所有线程完成
    for (int i = 0; i < 4; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i], NULL));
    }
    
    // 验证结果
    ASSERT_EQ(ppdb_skiplist_size(list), 400);  // 4个线程 * 100个键值对
    
    // 清理
    ppdb_skiplist_destroy(list);
    printf("Concurrent skiplist tests completed\n");
    return 0;
}

// 边界条件测试
static int test_skiplist_boundary(void) {
    printf("\n=== Running boundary condition tests ===\n");
    
    ppdb_skiplist_t* list = NULL;
    ASSERT_OK(ppdb_skiplist_create(&list));
    
    // 测试NULL参数
    ppdb_key_t key;
    ppdb_value_t value;
    ASSERT_ERR(ppdb_skiplist_insert(list, NULL, &value), PPDB_ERR_NULL_POINTER);
    ASSERT_ERR(ppdb_skiplist_insert(list, &key, NULL), PPDB_ERR_NULL_POINTER);
    
    // 测试空键/值
    create_test_kv("", "value", &key, &value);
    ASSERT_ERR(ppdb_skiplist_insert(list, &key, &value), PPDB_ERR_NULL_POINTER);
    free_test_kv(&key, &value);
    
    create_test_kv("key", "", &key, &value);
    ASSERT_ERR(ppdb_skiplist_insert(list, &key, &value), PPDB_ERR_NULL_POINTER);
    free_test_kv(&key, &value);
    
    // 测试重复键
    create_test_kv("key", "value1", &key, &value);
    ASSERT_OK(ppdb_skiplist_insert(list, &key, &value));
    
    ppdb_value_t value2;
    create_test_kv("key", "value2", &key, &value2);
    ASSERT_OK(ppdb_skiplist_insert(list, &key, &value2));  // 应该更新值
    
    ppdb_value_t found_value;
    ASSERT_OK(ppdb_skiplist_find(list, &key, &found_value));
    ASSERT_EQ(found_value.size, value2.size);
    ASSERT_EQ(memcmp(found_value.data, value2.data, value2.size), 0);
    
    // 测试删除不存在的键
    create_test_kv("nonexistent", "", &key, &value);
    ASSERT_ERR(ppdb_skiplist_remove(list, &key), PPDB_ERR_NOT_FOUND);
    
    // 清理
    free_test_kv(&key, &value);
    free_test_kv(&key, &value2);
    ppdb_skiplist_destroy(list);
    printf("Boundary condition tests completed\n");
    return 0;
}

// 压力测试
static int test_skiplist_stress(void) {
    printf("\n=== Running stress tests ===\n");
    
    ppdb_skiplist_t* list = NULL;
    ASSERT_OK(ppdb_skiplist_create(&list));
    
    // 大量数据插入
    const int num_entries = 10000;
    printf("Inserting %d entries...\n", num_entries);
    
    for (int i = 0; i < num_entries; i++) {
        char key_str[32], value_str[32];
        snprintf(key_str, sizeof(key_str), "key_%d", i);
        snprintf(value_str, sizeof(value_str), "value_%d", i);
        
        ppdb_key_t key;
        ppdb_value_t value;
        create_test_kv(key_str, value_str, &key, &value);
        
        ASSERT_OK(ppdb_skiplist_insert(list, &key, &value));
        
        free_test_kv(&key, &value);
        
        if (i % 1000 == 0) {
            printf("Inserted %d entries\n", i);
        }
    }
    
    // 验证所有数据
    printf("Verifying %d entries...\n", num_entries);
    for (int i = 0; i < num_entries; i++) {
        char key_str[32], value_str[32];
        snprintf(key_str, sizeof(key_str), "key_%d", i);
        snprintf(value_str, sizeof(value_str), "value_%d", i);
        
        ppdb_key_t key;
        ppdb_value_t value;
        create_test_kv(key_str, value_str, &key, &value);
        
        ppdb_value_t found_value;
        ASSERT_OK(ppdb_skiplist_find(list, &key, &found_value));
        ASSERT_EQ(found_value.size, value.size);
        ASSERT_EQ(memcmp(found_value.data, value.data, value.size), 0);
        
        free_test_kv(&key, &value);
        
        if (i % 1000 == 0) {
            printf("Verified %d entries\n", i);
        }
    }
    
    // 删除所有数据
    printf("Deleting %d entries...\n", num_entries);
    for (int i = 0; i < num_entries; i++) {
        char key_str[32];
        snprintf(key_str, sizeof(key_str), "key_%d", i);
        
        ppdb_key_t key;
        ppdb_value_t value;  // 仅用于create_test_kv
        create_test_kv(key_str, "", &key, &value);
        
        ASSERT_OK(ppdb_skiplist_remove(list, &key));
        
        free_test_kv(&key, &value);
        
        if (i % 1000 == 0) {
            printf("Deleted %d entries\n", i);
        }
    }
    
    // 验证大小
    ASSERT_EQ(ppdb_skiplist_size(list), 0);
    
    // 清理
    ppdb_skiplist_destroy(list);
    printf("Stress tests completed\n");
    return 0;
}

int main(void) {
    if (test_setup() != 0) {
        printf("Test setup failed\n");
        return 1;
    }
    
    TEST_CASE(test_skiplist_basic);
    TEST_CASE(test_skiplist_concurrent);
    TEST_CASE(test_skiplist_boundary);
    TEST_CASE(test_skiplist_stress);
    
    if (test_teardown() != 0) {
        printf("Test teardown failed\n");
        return 1;
    }
    
    printf("\nTest summary:\n");
    printf("  Total: %d\n", g_test_count);
    printf("  Passed: %d\n", g_test_passed);
    printf("  Failed: %d\n", g_test_failed);
    
    return g_test_failed > 0 ? 1 : 0;
}