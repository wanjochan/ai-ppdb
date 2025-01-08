#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"
#include "../test_framework.h"

// 全局测试数据
static ppdb_base_t* g_base = NULL;
static ppdb_storage_t* g_storage = NULL;

// 测试初始化
static int test_setup(void) {
    printf("\n=== Setting up memkv test environment ===\n");
    
    // 初始化 base 配置
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024 * 10,  // 10MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    
    // 初始化各层
    ASSERT_OK(ppdb_base_init(&g_base, &base_config));
    
    printf("Test environment setup completed\n");
    return 0;
}

// 测试清理
static int test_teardown(void) {
    printf("\n=== Cleaning up memkv test environment ===\n");
    
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
    memcpy(key->data, key_str, key->size);
    
    value->size = strlen(value_str);
    memcpy(value->data, value_str, value->size);
}

// 基础功能测试
static int test_memkv_basic(void) {
    printf("\n=== Running basic memkv tests ===\n");
    
    // 创建memkv实例
    ppdb_memkv_t* memkv = NULL;
    ASSERT_OK(ppdb_memkv_create(&memkv));
    
    // 准备测试数据
    ppdb_key_t key1;
    ppdb_value_t value1;
    create_test_kv("key1", "value1", &key1, &value1);
    
    // 测试写入
    ASSERT_OK(ppdb_memkv_put(memkv, &key1, &value1));
    
    // 测试读取
    ppdb_value_t found_value;
    ASSERT_OK(ppdb_memkv_get(memkv, &key1, &found_value));
    ASSERT_EQ(found_value.size, value1.size);
    ASSERT_EQ(memcmp(found_value.data, value1.data, value1.size), 0);
    
    // 测试删除
    ASSERT_OK(ppdb_memkv_delete(memkv, &key1));
    ASSERT_ERR(ppdb_memkv_get(memkv, &key1, &found_value), PPDB_ERR_NOT_FOUND);
    
    // 清理
    ppdb_memkv_destroy(memkv);
    printf("Basic memkv tests completed\n");
    return 0;
}

// 并发操作测试
typedef struct {
    ppdb_memkv_t* memkv;
    int thread_id;
} thread_data_t;

static void* concurrent_write_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    ppdb_memkv_t* memkv = data->memkv;
    int thread_id = data->thread_id;
    
    for (int i = 0; i < 100; i++) {
        char key_str[32], value_str[32];
        snprintf(key_str, sizeof(key_str), "key_%d_%d", thread_id, i);
        snprintf(value_str, sizeof(value_str), "value_%d", i);
        
        ppdb_key_t key;
        ppdb_value_t value;
        create_test_kv(key_str, value_str, &key, &value);
        
        ppdb_memkv_put(memkv, &key, &value);
    }
    
    return NULL;
}

static int test_memkv_concurrent(void) {
    printf("\n=== Running concurrent memkv tests ===\n");
    
    // 创建memkv实例
    ppdb_memkv_t* memkv = NULL;
    ASSERT_OK(ppdb_memkv_create(&memkv));
    
    // 创建多个线程进行并发写入
    pthread_t threads[4];
    thread_data_t thread_data[4];
    
    for (int i = 0; i < 4; i++) {
        thread_data[i].memkv = memkv;
        thread_data[i].thread_id = i;
        pthread_create(&threads[i], NULL, concurrent_write_thread, &thread_data[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 验证数据
    for (int t = 0; t < 4; t++) {
        for (int i = 0; i < 100; i++) {
            char key_str[32], value_str[32];
            snprintf(key_str, sizeof(key_str), "key_%d_%d", t, i);
            snprintf(value_str, sizeof(value_str), "value_%d", i);
            
            ppdb_key_t key;
            ppdb_value_t value;
            create_test_kv(key_str, value_str, &key, &value);
            
            ppdb_value_t found_value;
            ASSERT_OK(ppdb_memkv_get(memkv, &key, &found_value));
            ASSERT_EQ(found_value.size, value.size);
            ASSERT_EQ(memcmp(found_value.data, value.data, value.size), 0);
        }
    }
    
    // 清理
    ppdb_memkv_destroy(memkv);
    printf("Concurrent memkv tests completed\n");
    return 0;
}

// 边界条件测试
static int test_memkv_boundary(void) {
    printf("\n=== Running boundary condition tests ===\n");
    
    ppdb_memkv_t* memkv = NULL;
    ASSERT_OK(ppdb_memkv_create(&memkv));
    
    // 测试NULL参数
    ppdb_key_t key;
    ppdb_value_t value;
    ASSERT_ERR(ppdb_memkv_put(memkv, NULL, &value), PPDB_ERR_NULL_POINTER);
    ASSERT_ERR(ppdb_memkv_put(memkv, &key, NULL), PPDB_ERR_NULL_POINTER);
    
    // 测试空键/值
    create_test_kv("", "value", &key, &value);
    ASSERT_ERR(ppdb_memkv_put(memkv, &key, &value), PPDB_ERR_NULL_POINTER);
    
    create_test_kv("key", "", &key, &value);
    ASSERT_ERR(ppdb_memkv_put(memkv, &key, &value), PPDB_ERR_NULL_POINTER);
    
    // 测试重复键
    create_test_kv("key", "value1", &key, &value);
    ASSERT_OK(ppdb_memkv_put(memkv, &key, &value));
    
    ppdb_value_t value2;
    create_test_kv("key", "value2", &key, &value2);
    ASSERT_OK(ppdb_memkv_put(memkv, &key, &value2));  // 应该更新值
    
    ppdb_value_t found_value;
    ASSERT_OK(ppdb_memkv_get(memkv, &key, &found_value));
    ASSERT_EQ(found_value.size, value2.size);
    ASSERT_EQ(memcmp(found_value.data, value2.data, value2.size), 0);
    
    // 测试删除不存在的键
    create_test_kv("nonexistent", "", &key, &value);
    ASSERT_ERR(ppdb_memkv_delete(memkv, &key), PPDB_ERR_NOT_FOUND);
    
    // 清理
    ppdb_memkv_destroy(memkv);
    printf("Boundary condition tests completed\n");
    return 0;
}

// 压力测试
static int test_memkv_stress(void) {
    printf("\n=== Running stress tests ===\n");
    
    ppdb_memkv_t* memkv = NULL;
    ASSERT_OK(ppdb_memkv_create(&memkv));
    
    // 大量数据写入
    const int num_entries = 10000;
    printf("Writing %d entries...\n", num_entries);
    
    for (int i = 0; i < num_entries; i++) {
        char key_str[32], value_str[32];
        snprintf(key_str, sizeof(key_str), "key_%d", i);
        snprintf(value_str, sizeof(value_str), "value_%d", i);
        
        ppdb_key_t key;
        ppdb_value_t value;
        create_test_kv(key_str, value_str, &key, &value);
        
        ASSERT_OK(ppdb_memkv_put(memkv, &key, &value));
        
        if (i % 1000 == 0) {
            printf("Written %d entries\n", i);
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
        ASSERT_OK(ppdb_memkv_get(memkv, &key, &found_value));
        ASSERT_EQ(found_value.size, value.size);
        ASSERT_EQ(memcmp(found_value.data, value.data, value.size), 0);
        
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
        
        ASSERT_OK(ppdb_memkv_delete(memkv, &key));
        
        if (i % 1000 == 0) {
            printf("Deleted %d entries\n", i);
        }
    }
    
    // 验证所有数据已删除
    for (int i = 0; i < num_entries; i++) {
        char key_str[32];
        snprintf(key_str, sizeof(key_str), "key_%d", i);
        
        ppdb_key_t key;
        ppdb_value_t value;  // 仅用于create_test_kv
        create_test_kv(key_str, "", &key, &value);
        
        ppdb_value_t found_value;
        ASSERT_ERR(ppdb_memkv_get(memkv, &key, &found_value), PPDB_ERR_NOT_FOUND);
        
        if (i % 1000 == 0) {
            printf("Verified deletion of %d entries\n", i);
        }
    }
    
    // 清理
    ppdb_memkv_destroy(memkv);
    printf("Stress tests completed\n");
    return 0;
}

// 内存限制测试
static int test_memkv_memory_limit(void) {
    printf("\n=== Running memory limit tests ===\n");
    
    ppdb_memkv_t* memkv = NULL;
    ASSERT_OK(ppdb_memkv_create(&memkv));
    
    // 写入大量数据直到达到内存限制
    char* large_value = malloc(1024 * 1024);  // 1MB的值
    memset(large_value, 'A', 1024 * 1024 - 1);
    large_value[1024 * 1024 - 1] = '\0';
    
    int i = 0;
    while (1) {
        char key_str[32];
        snprintf(key_str, sizeof(key_str), "large_key_%d", i);
        
        ppdb_key_t key;
        ppdb_value_t value;
        create_test_kv(key_str, large_value, &key, &value);
        
        int ret = ppdb_memkv_put(memkv, &key, &value);
        
        if (ret == PPDB_ERR_NO_MEMORY) {
            printf("Memory limit reached after %d entries\n", i);
            break;
        }
        
        if (ret != PPDB_OK) {
            printf("Unexpected error: %d\n", ret);
            free(large_value);
            ppdb_memkv_destroy(memkv);
            return 1;
        }
        
        i++;
    }
    
    // 清理
    free(large_value);
    ppdb_memkv_destroy(memkv);
    printf("Memory limit tests completed\n");
    return 0;
}

int main(void) {
    if (test_setup() != 0) {
        printf("Test setup failed\n");
        return 1;
    }
    
    TEST_CASE(test_memkv_basic);
    TEST_CASE(test_memkv_concurrent);
    TEST_CASE(test_memkv_boundary);
    TEST_CASE(test_memkv_stress);
    TEST_CASE(test_memkv_memory_limit);
    
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