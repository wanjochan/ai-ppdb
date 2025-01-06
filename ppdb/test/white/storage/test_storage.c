#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"
#include "test_common.h"

// Test storage initialization and cleanup
void test_storage_init(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;

    // Initialize base layer
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));

    // Initialize storage layer
    ppdb_storage_config_t storage_config = {
        .memtable_size = PPDB_DEFAULT_MEMTABLE_SIZE,
        .block_size = PPDB_DEFAULT_BLOCK_SIZE,
        .cache_size = PPDB_DEFAULT_CACHE_SIZE,
        .write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE,
        .data_dir = PPDB_DEFAULT_DATA_DIR,
        .use_compression = PPDB_DEFAULT_USE_COMPRESSION,
        .sync_writes = PPDB_DEFAULT_SYNC_WRITES
    };
    ASSERT_OK(ppdb_storage_init(&storage, base, &storage_config));

    // Verify storage configuration
    ppdb_storage_config_t config;
    ASSERT_OK(ppdb_storage_get_config(storage, &config));
    ASSERT_EQ(config.memtable_size, storage_config.memtable_size);
    ASSERT_EQ(config.block_size, storage_config.block_size);
    ASSERT_EQ(config.cache_size, storage_config.cache_size);
    ASSERT_EQ(config.write_buffer_size, storage_config.write_buffer_size);
    ASSERT_STR_EQ(config.data_dir, storage_config.data_dir);
    ASSERT_EQ(config.use_compression, storage_config.use_compression);
    ASSERT_EQ(config.sync_writes, storage_config.sync_writes);

    // Cleanup
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

// Test table operations
void test_storage_table(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;

    // Initialize base layer
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));

    // Initialize storage layer
    ppdb_storage_config_t storage_config = {
        .memtable_size = PPDB_DEFAULT_MEMTABLE_SIZE,
        .block_size = PPDB_DEFAULT_BLOCK_SIZE,
        .cache_size = PPDB_DEFAULT_CACHE_SIZE,
        .write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE,
        .data_dir = PPDB_DEFAULT_DATA_DIR,
        .use_compression = PPDB_DEFAULT_USE_COMPRESSION,
        .sync_writes = PPDB_DEFAULT_SYNC_WRITES
    };
    ASSERT_OK(ppdb_storage_init(&storage, base, &storage_config));

    // Create table
    ASSERT_OK(ppdb_table_create(storage, "test_table"));

    // Try to create same table again
    ASSERT_EQ(ppdb_table_create(storage, "test_table"), PPDB_ERR_TABLE_EXISTS);

    // Drop table
    ASSERT_OK(ppdb_table_drop(storage, "test_table"));

    // Try to drop non-existent table
    ASSERT_EQ(ppdb_table_drop(storage, "non_existent"), PPDB_ERR_TABLE_NOT_FOUND);

    // Cleanup
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

// Test data operations
void test_storage_data(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;
    ppdb_storage_table_t* table = NULL;

    // Initialize base layer
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));

    // Initialize storage layer
    ppdb_storage_config_t storage_config = {
        .memtable_size = PPDB_DEFAULT_MEMTABLE_SIZE,
        .block_size = PPDB_DEFAULT_BLOCK_SIZE,
        .cache_size = PPDB_DEFAULT_CACHE_SIZE,
        .write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE,
        .data_dir = PPDB_DEFAULT_DATA_DIR,
        .use_compression = PPDB_DEFAULT_USE_COMPRESSION,
        .sync_writes = PPDB_DEFAULT_SYNC_WRITES
    };
    ASSERT_OK(ppdb_storage_init(&storage, base, &storage_config));

    // Create table
    ASSERT_OK(ppdb_table_create(storage, "test_table"));

    // Get table
    ASSERT_OK(ppdb_storage_get_table(storage, "test_table", &table));
    ASSERT_NOT_NULL(table);

    // Test data operations
    const char* key = "test_key";
    const char* value = "test_value";
    char buffer[256];
    size_t size;

    // Put data
    ASSERT_OK(ppdb_storage_put(table, key, strlen(key), value, strlen(value)));

    // Get data
    size = sizeof(buffer);
    ASSERT_OK(ppdb_storage_get(table, key, strlen(key), buffer, &size));
    ASSERT_EQ(size, strlen(value));
    ASSERT_STR_EQ(buffer, value);

    // Delete data
    ASSERT_OK(ppdb_storage_delete(table, key, strlen(key)));

    // Try to get deleted data
    size = sizeof(buffer);
    ASSERT_EQ(ppdb_storage_get(table, key, strlen(key), buffer, &size), PPDB_ERR_NOT_FOUND);

    // Drop table
    ASSERT_OK(ppdb_table_drop(storage, "test_table"));

    // Cleanup
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

// Test memkv operations
void test_storage_memkv(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;

    // Initialize base layer
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));

    // Initialize storage layer with memkv engine
    ppdb_storage_config_t storage_config = {
        .engine = "memkv",
        .memtable_size = 0,  // Not used by memkv
        .block_size = 0,     // Not used by memkv
        .cache_size = 1024 * 1024,  // 1MB
        .write_buffer_size = 0,  // Not used by memkv
        .data_dir = NULL,    // Not used by memkv
        .use_compression = false,
        .sync_writes = false
    };
    ASSERT_OK(ppdb_storage_init(&storage, base, &storage_config));

    // Test basic operations
    ppdb_data_t key1 = {
        .data = (uint8_t*)"key1",
        .size = 4
    };
    ppdb_data_t value1 = {
        .data = (uint8_t*)"value1",
        .size = 6
    };
    ppdb_data_t key2 = {
        .data = (uint8_t*)"key2",
        .size = 4
    };
    ppdb_data_t value2 = {
        .data = (uint8_t*)"value2",
        .size = 6
    };

    // Test put
    ASSERT_OK(ppdb_storage_put(storage, &key1, &value1));
    ASSERT_OK(ppdb_storage_put(storage, &key2, &value2));

    // Test get
    ppdb_data_t result;
    ASSERT_OK(ppdb_storage_get(storage, &key1, &result));
    ASSERT_EQ(result.size, value1.size);
    ASSERT_MEM_EQ(result.data, value1.data, value1.size);

    ASSERT_OK(ppdb_storage_get(storage, &key2, &result));
    ASSERT_EQ(result.size, value2.size);
    ASSERT_MEM_EQ(result.data, value2.data, value2.size);

    // Test delete
    ASSERT_OK(ppdb_storage_delete(storage, &key1));
    ASSERT_EQ(ppdb_storage_get(storage, &key1, &result), PPDB_ERR_NOT_FOUND);
    ASSERT_OK(ppdb_storage_get(storage, &key2, &result));

    // Test clear
    ASSERT_OK(ppdb_storage_clear(storage));
    ASSERT_EQ(ppdb_storage_get(storage, &key2, &result), PPDB_ERR_NOT_FOUND);

    // Test stats
    char stats_buffer[1024];
    ASSERT_OK(ppdb_storage_get_stats(storage, stats_buffer, sizeof(stats_buffer)));
    ASSERT_TRUE(strstr(stats_buffer, "STAT curr_items 0") != NULL);
    ASSERT_TRUE(strstr(stats_buffer, "STAT bytes 0") != NULL);

    // Test memory limit
    ppdb_data_t big_key = {
        .data = (uint8_t*)"big_key",
        .size = 7
    };
    ppdb_data_t big_value;
    big_value.size = 2 * 1024 * 1024;  // 2MB, larger than memory limit
    big_value.data = malloc(big_value.size);
    memset(big_value.data, 'x', big_value.size);

    // This should trigger eviction
    ASSERT_OK(ppdb_storage_put(storage, &big_key, &big_value));
    ASSERT_OK(ppdb_storage_get_stats(storage, stats_buffer, sizeof(stats_buffer)));
    ASSERT_TRUE(strstr(stats_buffer, "STAT evictions") != NULL);

    free(big_value.data);

    // Cleanup
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

// 测试 LRU 驱逐策略
static void test_memkv_lru_eviction(void) {
    ppdb_base_t* base = NULL;
    storage_memkv_t* kv = NULL;
    
    // 初始化，设置较小的内存限制以触发驱逐
    ppdb_base_config_t base_config = {
        .memory_limit = 1024,  // 1KB
        .thread_pool_size = 1,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));
    
    ppdb_options_t options = {
        .cache_size = 1024  // 1KB
    };
    ASSERT_OK(memkv_init((void**)&kv, &options));
    
    // 写入数据直到触发驱逐
    char key[32], value[256];
    for (int i = 0; i < 10; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        memset(value, 'v', sizeof(value) - 1);
        value[sizeof(value) - 1] = '\0';
        
        ppdb_data_t k = {(uint8_t*)key, strlen(key)};
        ppdb_data_t v = {(uint8_t*)value, strlen(value)};
        
        ASSERT_OK(memkv_put(kv, &k, &v));
        
        // 访问一些键以更新其LRU状态
        if (i > 0 && i % 2 == 0) {
            snprintf(key, sizeof(key), "key_%d", i-1);
            k.data = (uint8_t*)key;
            k.size = strlen(key);
            
            ppdb_data_t result;
            ASSERT_OK(memkv_get(kv, &k, &result));
            free(result.data);
        }
    }
    
    // 验证最早未访问的键已被驱逐
    ppdb_data_t k = {(uint8_t*)"key_0", 5};
    ppdb_data_t result;
    ASSERT_EQ(memkv_get(kv, &k, &result), PPDB_ERR_NOT_FOUND);
    
    // 验证最近访问的键仍然存在
    snprintf(key, sizeof(key), "key_8");
    k.data = (uint8_t*)key;
    k.size = strlen(key);
    ASSERT_OK(memkv_get(kv, &k, &result));
    free(result.data);
    
    // 清理
    memkv_destroy(kv);
    ppdb_base_destroy(base);
}

// 测试边界条件
static void test_memkv_edge_cases(void) {
    ppdb_base_t* base = NULL;
    storage_memkv_t* kv = NULL;
    
    // 初始化
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,
        .thread_pool_size = 1,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));
    
    ppdb_options_t options = {
        .cache_size = 1024 * 1024
    };
    ASSERT_OK(memkv_init((void**)&kv, &options));
    
    // 测试空键/值
    ppdb_data_t empty_key = {NULL, 0};
    ppdb_data_t empty_value = {NULL, 0};
    ASSERT_EQ(memkv_put(kv, &empty_key, &empty_value), PPDB_ERR_PARAM);
    ASSERT_EQ(memkv_get(kv, &empty_key, &empty_value), PPDB_ERR_PARAM);
    
    // 测试最大键/值大小
    uint8_t* large_key = malloc(1024 * 1024);
    uint8_t* large_value = malloc(1024 * 1024);
    memset(large_key, 'k', 1024 * 1024 - 1);
    memset(large_value, 'v', 1024 * 1024 - 1);
    large_key[1024 * 1024 - 1] = '\0';
    large_value[1024 * 1024 - 1] = '\0';
    
    ppdb_data_t k = {large_key, 1024 * 1024};
    ppdb_data_t v = {large_value, 1024 * 1024};
    
    // 这应该触发内存限制
    ASSERT_EQ(memkv_put(kv, &k, &v), PPDB_ERR_MEMORY);
    
    free(large_key);
    free(large_value);
    
    // 测试重复键
    uint8_t* key = (uint8_t*)"test_key";
    uint8_t* value1 = (uint8_t*)"value1";
    uint8_t* value2 = (uint8_t*)"value2";
    
    k.data = key;
    k.size = strlen((char*)key);
    v.data = value1;
    v.size = strlen((char*)value1);
    
    ASSERT_OK(memkv_put(kv, &k, &v));
    
    v.data = value2;
    v.size = strlen((char*)value2);
    
    // 更新已存在的键
    ASSERT_OK(memkv_put(kv, &k, &v));
    
    // 验证值已更新
    ppdb_data_t result;
    ASSERT_OK(memkv_get(kv, &k, &result));
    ASSERT_EQ(result.size, strlen((char*)value2));
    ASSERT_MEM_EQ(result.data, value2, result.size);
    free(result.data);
    
    // 清理
    memkv_destroy(kv);
    ppdb_base_destroy(base);
}

// 并发测试参数
#define NUM_THREADS 4
#define OPS_PER_THREAD 1000

typedef struct {
    storage_memkv_t* kv;
    int thread_id;
    int success_count;
} thread_ctx_t;

// 并发测试线程函数
static void* concurrent_worker(void* arg) {
    thread_ctx_t* ctx = (thread_ctx_t*)arg;
    char key[32], value[32];
    
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        // 生成测试数据
        snprintf(key, sizeof(key), "key_%d_%d", ctx->thread_id, i);
        snprintf(value, sizeof(value), "value_%d_%d", ctx->thread_id, i);
        
        ppdb_data_t k = {(uint8_t*)key, strlen(key)};
        ppdb_data_t v = {(uint8_t*)value, strlen(value)};
        
        // 写入数据
        if (memkv_put(ctx->kv, &k, &v) == PPDB_OK) {
            ctx->success_count++;
        }
        
        // 随机读取之前写入的数据
        if (i > 0 && i % 2 == 0) {
            int read_idx = rand() % i;
            snprintf(key, sizeof(key), "key_%d_%d", ctx->thread_id, read_idx);
            k.data = (uint8_t*)key;
            k.size = strlen(key);
            
            ppdb_data_t result;
            if (memkv_get(ctx->kv, &k, &result) == PPDB_OK) {
                ctx->success_count++;
                free(result.data);
            }
        }
        
        // 随机删除一些数据
        if (i > 0 && i % 5 == 0) {
            int del_idx = rand() % i;
            snprintf(key, sizeof(key), "key_%d_%d", ctx->thread_id, del_idx);
            k.data = (uint8_t*)key;
            k.size = strlen(key);
            
            if (memkv_delete(ctx->kv, &k) == PPDB_OK) {
                ctx->success_count++;
            }
        }
    }
    
    return NULL;
}

// 测试并发操作
static void test_memkv_concurrent(void) {
    ppdb_base_t* base = NULL;
    storage_memkv_t* kv = NULL;
    
    // 初始化
    ppdb_base_config_t base_config = {
        .memory_limit = 10 * 1024 * 1024,  // 10MB
        .thread_pool_size = NUM_THREADS,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));
    
    ppdb_options_t options = {
        .cache_size = 10 * 1024 * 1024  // 10MB
    };
    ASSERT_OK(memkv_init((void**)&kv, &options));
    
    // 创建线程
    ppdb_base_thread_t* threads[NUM_THREADS];
    thread_ctx_t thread_ctx[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ctx[i].kv = kv;
        thread_ctx[i].thread_id = i;
        thread_ctx[i].success_count = 0;
        
        ASSERT_OK(ppdb_base_thread_create(&threads[i], concurrent_worker, &thread_ctx[i]));
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i], NULL));
        printf("Thread %d completed with %d successful operations\n", 
               i, thread_ctx[i].success_count);
    }
    
    // 验证数据一致性
    char key[32];
    int found_count = 0;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            snprintf(key, sizeof(key), "key_%d_%d", t, i);
            ppdb_data_t k = {(uint8_t*)key, strlen(key)};
            ppdb_data_t result;
            
            if (memkv_get(kv, &k, &result) == PPDB_OK) {
                found_count++;
                free(result.data);
            }
        }
    }
    
    printf("Total found keys after concurrent operations: %d\n", found_count);
    
    // 获取并打印统计信息
    char stats_buf[1024];
    ASSERT_OK(memkv_get_stats(kv, stats_buf, sizeof(stats_buf)));
    printf("Final stats:\n%s", stats_buf);
    
    // 清理
    memkv_destroy(kv);
    ppdb_base_destroy(base);
}

// 测试基本存储操作
static void test_storage_basic(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;
    
    // 初始化基础设施
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 1,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));
    
    // 初始化存储层
    ppdb_storage_config_t storage_config = {
        .memtable_size = 1024 * 1024,  // 1MB
        .sync_writes = false
    };
    ASSERT_OK(ppdb_storage_init(&storage, base, &storage_config));

    // 测试基本操作
    const char* key1_str = "key1";
    const char* value1_str = "value1";
    const char* key2_str = "key2";
    const char* value2_str = "value2";

    // 写入测试
    ASSERT_OK(ppdb_storage_put(storage, key1_str, strlen(key1_str), value1_str, strlen(value1_str)));
    ASSERT_OK(ppdb_storage_put(storage, key2_str, strlen(key2_str), value2_str, strlen(value2_str)));

    // 读取测试
    char value_buf[32];
    size_t value_size;
    
    ASSERT_OK(ppdb_storage_get(storage, key1_str, strlen(key1_str), value_buf, &value_size));
    ASSERT_EQ(value_size, strlen(value1_str));
    ASSERT_MEM_EQ(value_buf, value1_str, value_size);

    ASSERT_OK(ppdb_storage_get(storage, key2_str, strlen(key2_str), value_buf, &value_size));
    ASSERT_EQ(value_size, strlen(value2_str));
    ASSERT_MEM_EQ(value_buf, value2_str, value_size);

    // 删除测试
    ASSERT_OK(ppdb_storage_delete(storage, key1_str, strlen(key1_str)));
    ASSERT_EQ(ppdb_storage_get(storage, key1_str, strlen(key1_str), value_buf, &value_size), PPDB_ERR_NOT_FOUND);
    ASSERT_OK(ppdb_storage_get(storage, key2_str, strlen(key2_str), value_buf, &value_size));

    // 清空测试
    ASSERT_OK(ppdb_storage_flush(storage));
    ASSERT_EQ(ppdb_storage_get(storage, key2_str, strlen(key2_str), value_buf, &value_size), PPDB_ERR_NOT_FOUND);

    // 统计信息测试
    ppdb_storage_stats_t stats;
    ASSERT_OK(ppdb_storage_get_stats(storage, &stats));
    ASSERT_EQ(stats.total_items, 0);
    ASSERT_EQ(stats.total_memory, 0);

    // 测试内存限制
    char* big_value = malloc(2 * 1024 * 1024);  // 2MB
    memset(big_value, 'x', 2 * 1024 * 1024 - 1);
    big_value[2 * 1024 * 1024 - 1] = '\0';

    const char* big_key = "big_key";
    ASSERT_EQ(ppdb_storage_put(storage, big_key, strlen(big_key), big_value, 2 * 1024 * 1024), PPDB_ERR_MEMORY);

    free(big_value);

    // 清理
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

int main(void) {
    TEST_INIT();
    
    RUN_TEST(test_storage_basic);
    
    TEST_SUMMARY();
    return TEST_RESULT();
} 