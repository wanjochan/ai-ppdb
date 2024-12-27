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
    ppdb_log_info("Testing MemTable basic operations...");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(4096, &table);
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    
    // Put key-value pair
    const uint8_t* key = (const uint8_t*)"test_key";
    const uint8_t* value = (const uint8_t*)"test_value";
    err = ppdb_memtable_put(table, key, strlen((const char*)key), value, strlen((const char*)value));
    TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    
    // Get value
    uint8_t buf[256] = {0};
    size_t size = sizeof(buf);  // 设置足够大的缓冲区大小
    err = ppdb_memtable_get(table, key, strlen((const char*)key), buf, &size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value");
    TEST_ASSERT(size == strlen((const char*)value), "Value size mismatch");
    TEST_ASSERT(memcmp(buf, value, size) == 0, "Value content mismatch");
    
    // 测试缓冲区太小的情况
    uint8_t small_buf[4] = {0};
    size_t small_size = sizeof(small_buf);
    err = ppdb_memtable_get(table, key, strlen((const char*)key), small_buf, &small_size);
    TEST_ASSERT(err == PPDB_ERR_BUFFER_TOO_SMALL, "Should fail with buffer too small");
    TEST_ASSERT(small_size == strlen((const char*)value), "Should return required buffer size");
    
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
    uint8_t buf[256] = {0};
    size_t size = 0;
    err = ppdb_memtable_get(table, key, strlen((const char*)key), buf, &size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should not exist after deletion");
    
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
        const uint8_t* key = ppdb_memtable_iterator_key(iter);
        const uint8_t* value = ppdb_memtable_iterator_value(iter);
        TEST_ASSERT(key != NULL && value != NULL, "Iterator key/value is NULL");
        count++;
        ppdb_memtable_iterator_next(iter);
    }
    
    TEST_ASSERT(count == sizeof(pairs) / sizeof(pairs[0]), "Iterator count mismatch");
    
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