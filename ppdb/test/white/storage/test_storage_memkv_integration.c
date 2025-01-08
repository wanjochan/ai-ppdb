#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "unity.h"
#include "storage/memkv.h"
#include "storage/skiplist_lockfree.h"

// Global test data
static memkv_t* kv_store;
static const char* test_keys[] = {"key1", "key2", "key3"};
static const char* test_values[] = {"value1", "value2", "value3"};

// Test setup and teardown
void setUp(void) {
    kv_store = memkv_create();
    TEST_ASSERT_NOT_NULL(kv_store);
}

void tearDown(void) {
    memkv_destroy(kv_store);
}

// Test cases
void test_memkv_integration_basic_operations(void) {
    // Test put operation
    TEST_ASSERT_EQUAL(0, memkv_put(kv_store, test_keys[0], test_values[0]));
    
    // Test get operation
    char* value = memkv_get(kv_store, test_keys[0]);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_STRING(test_values[0], value);
    free(value);
    
    // Test delete operation
    TEST_ASSERT_EQUAL(0, memkv_delete(kv_store, test_keys[0]));
    value = memkv_get(kv_store, test_keys[0]);
    TEST_ASSERT_NULL(value);
}

void test_memkv_integration_multiple_operations(void) {
    // Insert multiple key-value pairs
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL(0, memkv_put(kv_store, test_keys[i], test_values[i]));
    }
    
    // Verify all values
    for (int i = 0; i < 3; i++) {
        char* value = memkv_get(kv_store, test_keys[i]);
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL_STRING(test_values[i], value);
        free(value);
    }
}

void test_memkv_integration_update_operations(void) {
    // Insert and update
    TEST_ASSERT_EQUAL(0, memkv_put(kv_store, test_keys[0], test_values[0]));
    TEST_ASSERT_EQUAL(0, memkv_put(kv_store, test_keys[0], test_values[1]));
    
    // Verify updated value
    char* value = memkv_get(kv_store, test_keys[0]);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_STRING(test_values[1], value);
    free(value);
}

// Main function
int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_memkv_integration_basic_operations);
    RUN_TEST(test_memkv_integration_multiple_operations);
    RUN_TEST(test_memkv_integration_update_operations);
    
    return UNITY_END();
}