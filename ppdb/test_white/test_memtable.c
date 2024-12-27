#include <cosmopolitan.h>
#include "test_framework.h"
#include <ppdb/memtable.h>
#include <ppdb/error.h>

// 创建和销毁测试
static int test_memtable_create_destroy(void) {
    ppdb_log_info("Testing MemTable create/destroy...");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(4096, &table);
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    TEST_ASSERT(table != NULL, "MemTable pointer is NULL");
    
    ppdb_memtable_destroy(table);
    return 0;
}

// 基本操作测试
static int test_memtable_basic_ops(void) {
    ppdb_log_info("Testing MemTable basic operations...");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(4096, &table);
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    
    // 测试插入
    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_memtable_put(table, key, strlen(key), value, strlen(value));
    TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    
    // 测试查询
    char buf[256] = {0};
    size_t size = 0;
    err = ppdb_memtable_get(table, key, strlen(key), buf, sizeof(buf), &size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value");
    TEST_ASSERT(strcmp(buf, value) == 0, "Retrieved value does not match");
    
    ppdb_memtable_destroy(table);
    return 0;
}

// 删除操作测试
static int test_memtable_delete(void) {
    ppdb_log_info("Testing MemTable delete operation...");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(4096, &table);
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    
    // 插入然后删除
    const char* key = "delete_key";
    const char* value = "delete_value";
    err = ppdb_memtable_put(table, key, strlen(key), value, strlen(value));
    TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    
    err = ppdb_memtable_delete(table, key, strlen(key));
    TEST_ASSERT(err == PPDB_OK, "Failed to delete key");
    
    // 验证删除
    char buf[256] = {0};
    size_t size = 0;
    err = ppdb_memtable_get(table, key, strlen(key), buf, sizeof(buf), &size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key still exists after deletion");
    
    ppdb_memtable_destroy(table);
    return 0;
}

// 大小限制测试
static int test_memtable_size_limit(void) {
    ppdb_log_info("Testing MemTable size limit...");
    
    // 创建一个很小的MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(32, &table);  // 只允许32字节
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    
    // 尝试插入超过限制的数据
    const char* key = "size_limit_key";
    const char* value = "this_is_a_very_long_value_that_should_exceed_the_size_limit";
    err = ppdb_memtable_put(table, key, strlen(key), value, strlen(value));
    TEST_ASSERT(err == PPDB_ERR_FULL, "Should fail with FULL error");
    
    ppdb_memtable_destroy(table);
    return 0;
}

// 迭代器测试
static int test_memtable_iterator(void) {
    ppdb_log_info("Testing MemTable iterator...");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(4096, &table);
    TEST_ASSERT(err == PPDB_OK, "Failed to create MemTable");
    
    // 插入一些数据
    const char* pairs[][2] = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"}
    };
    
    for (int i = 0; i < 3; i++) {
        err = ppdb_memtable_put(table, pairs[i][0], strlen(pairs[i][0]), 
                               pairs[i][1], strlen(pairs[i][1]));
        TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    }
    
    // 使用迭代器遍历
    ppdb_memtable_iterator_t* iter = NULL;
    err = ppdb_memtable_iterator_create(table, &iter);
    TEST_ASSERT(err == PPDB_OK, "Failed to create iterator");
    
    int count = 0;
    while (ppdb_memtable_iterator_valid(iter)) {
        const char* key = ppdb_memtable_iterator_key(iter);
        const char* value = ppdb_memtable_iterator_value(iter);
        TEST_ASSERT(key != NULL && value != NULL, "Iterator key/value is NULL");
        count++;
        ppdb_memtable_iterator_next(iter);
    }
    
    TEST_ASSERT(count == 3, "Iterator did not visit all items");
    
    ppdb_memtable_iterator_destroy(iter);
    ppdb_memtable_destroy(table);
    return 0;
}

// MemTable测试套件定义
static const test_case_t memtable_test_cases[] = {
    {"create_destroy", test_memtable_create_destroy},
    {"basic_ops", test_memtable_basic_ops},
    {"delete", test_memtable_delete},
    {"size_limit", test_memtable_size_limit},
    {"iterator", test_memtable_iterator}
};

// 导出MemTable测试套件
const test_suite_t memtable_suite = {
    .name = "MemTable",
    .cases = memtable_test_cases,
    .num_cases = sizeof(memtable_test_cases) / sizeof(memtable_test_cases[0])
}; 