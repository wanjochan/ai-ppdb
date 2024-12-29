#include <cosmopolitan.h>
#include "test_framework.h"
#include <ppdb/memtable.h>
#include <ppdb/error.h>
#include <ppdb/logger.h>

// Test MemTable create/destroy
static int test_memtable_create_destroy(void) {
    ppdb_log_info("Testing MemTable create/destroy...");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(4096, &table);
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    TEST_ASSERT(table != NULL, "MemTable pointer is NULL");
    
    ppdb_memtable_destroy(table);
    return 0;
}

// Test MemTable basic operations
static int test_memtable_basic_ops(void) {
    printf("Testing MemTable basic operations...\n");

    // 创建 MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(4096, &table);
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    TEST_ASSERT(table != NULL, "MemTable pointer is NULL");

    // 测试插入和获取
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    err = ppdb_memtable_put(table, (const uint8_t*)test_key, strlen(test_key),
                           (const uint8_t*)test_value, strlen(test_value));
    TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");

    // 先获取值的大小
    size_t value_size = 0;
    err = ppdb_memtable_get(table, (const uint8_t*)test_key, strlen(test_key),
                           NULL, &value_size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value size");
    TEST_ASSERT(value_size == strlen(test_value), "Value size mismatch");

    // 获取值
    uint8_t* value_buf = NULL;
    size_t actual_size = 0;
    err = ppdb_memtable_get(table, (const uint8_t*)test_key, strlen(test_key),
                           &value_buf, &actual_size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value");
    TEST_ASSERT(actual_size == strlen(test_value), "Value size mismatch");
    TEST_ASSERT(value_buf != NULL, "Value buffer is NULL");
    TEST_ASSERT(memcmp(value_buf, test_value, actual_size) == 0, "Value content mismatch");
    free(value_buf);

    // 测试删除
    err = ppdb_memtable_delete(table, (const uint8_t*)test_key, strlen(test_key));
    TEST_ASSERT(err == PPDB_OK, "Failed to delete key");

    // 验证删除后无法获取
    err = ppdb_memtable_get(table, (const uint8_t*)test_key, strlen(test_key),
                           NULL, &value_size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key still exists after deletion");

    // 销毁 MemTable
    ppdb_memtable_destroy(table);
    return 0;
}

// Test MemTable delete operation
static int test_memtable_delete(void) {
    ppdb_log_info("Testing MemTable delete operation...");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(4096, &table);
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    
    // Put key-value pair
    const uint8_t* key = (const uint8_t*)"test_key";
    const uint8_t* value = (const uint8_t*)"test_value";
    err = ppdb_memtable_put(table, key, strlen((const char*)key), value, strlen((const char*)value));
    TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    
    // Delete key
    err = ppdb_memtable_delete(table, key, strlen((const char*)key));
    TEST_ASSERT(err == PPDB_OK, "Failed to delete key");
    
    // Try to get deleted key
    uint8_t* buf = NULL;
    size_t size = 0;
    err = ppdb_memtable_get(table, key, strlen((const char*)key), &buf, &size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should not exist after deletion");
    if (buf) free(buf);  // 如果获取成功，释放内存
    
    ppdb_memtable_destroy(table);
    return 0;
}

// Test MemTable size limit
static int test_memtable_size_limit(void) {
    ppdb_log_info("Testing MemTable size limit...");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(32, &table);  // Small size limit
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    
    // Put key-value pair
    const uint8_t* key = (const uint8_t*)"test_key";
    const uint8_t* value = (const uint8_t*)"test_value";
    err = ppdb_memtable_put(table, key, strlen((const char*)key), value, strlen((const char*)value));
    TEST_ASSERT(err == PPDB_ERR_FULL, "Should fail due to size limit");
    
    // Try to get the key (should not exist)
    uint8_t* buf = NULL;
    size_t size = 0;
    err = ppdb_memtable_get(table, key, strlen((const char*)key), &buf, &size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should not exist");
    if (buf) free(buf);  // 如果获取成功，释放内存
    
    ppdb_memtable_destroy(table);
    return 0;
}

// Test MemTable iterator
static int test_memtable_iterator(void) {
    ppdb_log_info("Testing MemTable iterator...");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(4096, &table);
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    
    // Insert some key-value pairs
    const char* pairs[][2] = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"}
    };
    
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
        err = ppdb_memtable_put(table, 
                               (const uint8_t*)pairs[i][0], strlen(pairs[i][0]), 
                               (const uint8_t*)pairs[i][1], strlen(pairs[i][1]));
        TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    }
    
    // Create iterator
    ppdb_memtable_iterator_t* iter = NULL;
    err = ppdb_memtable_iterator_create(table, &iter);
    TEST_ASSERT(err == PPDB_OK, "Failed to create iterator");
    
    // Iterate through all entries
    size_t count = 0;
    while (ppdb_memtable_iterator_valid(iter)) {
        const uint8_t* key = NULL;
        size_t key_len = 0;
        const uint8_t* value = NULL;
        size_t value_len = 0;
        
        err = ppdb_memtable_iterator_get(iter, &key, &key_len, &value, &value_len);
        TEST_ASSERT(err == PPDB_OK, "Failed to get key-value pair from iterator");
        TEST_ASSERT(key != NULL && value != NULL, "Iterator key/value is NULL");
        
        // Verify key and value
        bool found = false;
        for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
            if (key_len == strlen(pairs[i][0]) &&
                value_len == strlen(pairs[i][1]) &&
                memcmp(key, pairs[i][0], key_len) == 0 &&
                memcmp(value, pairs[i][1], value_len) == 0) {
                found = true;
                count++;
                break;
            }
        }
        TEST_ASSERT(found, "Unexpected key-value pair in iterator");
        
        ppdb_memtable_iterator_next(iter);
    }
    
    TEST_ASSERT(count == sizeof(pairs) / sizeof(pairs[0]), "Not all pairs were iterated");
    
    ppdb_memtable_iterator_destroy(iter);
    ppdb_memtable_destroy(table);
    return 0;
}

// MemTable test suite definition
static const test_case_t memtable_test_cases[] = {
    {"create_destroy", test_memtable_create_destroy},
    {"basic_ops", test_memtable_basic_ops},
    {"delete", test_memtable_delete},
    {"size_limit", test_memtable_size_limit},
    {"iterator", test_memtable_iterator}
};

// Export MemTable test suite
const test_suite_t memtable_suite = {
    .name = "MemTable",
    .cases = memtable_test_cases,
    .num_cases = sizeof(memtable_test_cases) / sizeof(memtable_test_cases[0])
};