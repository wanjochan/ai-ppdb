#include <cosmopolitan.h>
#include "../include/ppdb/memtable.h"
#include "../include/ppdb/error.h"
#include "test_framework.h"

// 测试函数声明
static int test_create(void);
static int test_basic_ops(void);
static int test_delete(void);
static int test_size_limit(void);

// 测试用例定义
static const test_case_t memtable_test_cases[] = {
    {"create", test_create},
    {"basic_ops", test_basic_ops},
    {"delete", test_delete},
    {"size_limit", test_size_limit}
};

// 测试套件定义
const test_suite_t memtable_suite = {
    .name = "MemTable",
    .cases = memtable_test_cases,
    .num_cases = sizeof(memtable_test_cases) / sizeof(memtable_test_cases[0])
};

// 测试创建和销毁
static int test_create() {
    printf("Testing create/destroy...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    printf("  Create result: %s\n", err == PPDB_OK ? "OK" : "Failed");
    if (err != PPDB_OK) return 1;
    
    ppdb_memtable_destroy(table);
    printf("  Destroy completed\n");
    return 0;
}

// 测试基本操作
static int test_basic_ops() {
    printf("Testing basic operations...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    if (err != PPDB_OK) return 1;
    
    // 测试写入
    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_memtable_put(table, (uint8_t*)key, strlen(key) + 1,
                           (uint8_t*)value, strlen(value) + 1);
    printf("  Put [key='%s', value='%s']: %s\n", key, value,
           err == PPDB_OK ? "OK" : "Failed");
    if (err != PPDB_OK) return 1;
    
    // 测试读取
    uint8_t read_buf[256];
    size_t read_len = sizeof(read_buf);
    err = ppdb_memtable_get(table, (uint8_t*)key, strlen(key) + 1,
                           read_buf, &read_len);
    printf("  Get [key='%s']: %s\n", key,
           err == PPDB_OK ? "OK" : "Failed");
    if (err != PPDB_OK) return 1;
    
    printf("  Value comparison: %s\n",
           strcmp((char*)read_buf, value) == 0 ? "OK" : "Failed");
    if (strcmp((char*)read_buf, value) != 0) return 1;
    
    ppdb_memtable_destroy(table);
    return 0;
}

// 测试删除
static int test_delete() {
    printf("Testing delete...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    if (err != PPDB_OK) return 1;
    
    // 写入数据
    const char* key = "delete_key";
    const char* value = "delete_value";
    err = ppdb_memtable_put(table, (uint8_t*)key, strlen(key) + 1,
                           (uint8_t*)value, strlen(value) + 1);
    if (err != PPDB_OK) return 1;
    
    // 删除数据
    err = ppdb_memtable_delete(table, (uint8_t*)key, strlen(key) + 1);
    printf("  Delete [key='%s']: %s\n", key,
           err == PPDB_OK ? "OK" : "Failed");
    if (err != PPDB_OK) return 1;
    
    // 验证删除
    uint8_t read_buf[256];
    size_t read_len = sizeof(read_buf);
    err = ppdb_memtable_get(table, (uint8_t*)key, strlen(key) + 1,
                           read_buf, &read_len);
    printf("  Verify delete [key='%s']: %s\n", key,
           err == PPDB_ERR_NOT_FOUND ? "OK" : "Failed");
    if (err != PPDB_ERR_NOT_FOUND) return 1;
    
    ppdb_memtable_destroy(table);
    return 0;
}

// 测试大小限制
static int test_size_limit() {
    printf("Testing size limit...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(32, &table);  // 小容量
    if (err != PPDB_OK) return 1;
    
    // 尝试写入大于容量的数据
    const char* key = "big_key";
    const char* value = "this_is_a_very_long_value_that_exceeds_the_size_limit";
    err = ppdb_memtable_put(table, (uint8_t*)key, strlen(key) + 1,
                           (uint8_t*)value, strlen(value) + 1);
    printf("  Result: %s\n",
           err == PPDB_ERR_FULL ? "Correctly rejected" : "Incorrectly accepted");
    if (err != PPDB_ERR_FULL) return 1;
    
    ppdb_memtable_destroy(table);
    return 0;
} 