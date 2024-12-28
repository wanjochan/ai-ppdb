#include <cosmopolitan.h>
#include "ppdb/logger.h"
#include "src/kvstore/memtable.h"

// 测试MemTable创建和销毁
static bool test_create_destroy() {
    printf("Testing MemTable create/destroy...\n");
    
    // 创建MemTable
    memtable_t* table = memtable_create();
    if (!table) {
        printf("Failed to create MemTable\n");
        return false;
    }
    
    // 销毁MemTable
    memtable_destroy(table);
    return true;
}

// 测试MemTable基本操作
static bool test_basic_ops() {
    printf("Testing MemTable basic operations...\n");
    
    // 创建MemTable
    memtable_t* table = memtable_create();
    if (!table) {
        printf("Failed to create MemTable\n");
        return false;
    }
    
    // 插入键值对
    const char* key = "test_key";
    const char* value = "test_value";
    if (!memtable_put(table, key, strlen(key), value, strlen(value) + 1)) {
        printf("Failed to put key-value pair\n");
        memtable_destroy(table);
        return false;
    }
    
    // 查找键值对
    void* found_value;
    uint32_t found_len;
    if (!memtable_get(table, key, strlen(key), &found_value, &found_len)) {
        printf("Failed to get key-value pair\n");
        memtable_destroy(table);
        return false;
    }
    
    // 验证值
    if (found_len != strlen(value) + 1 || strcmp(found_value, value) != 0) {
        printf("Value mismatch\n");
        memtable_destroy(table);
        return false;
    }
    
    // 销毁MemTable
    memtable_destroy(table);
    return true;
}

// 测试MemTable删除操作
static bool test_delete() {
    printf("Testing MemTable delete operation...\n");
    
    // 创建MemTable
    memtable_t* table = memtable_create();
    if (!table) {
        printf("Failed to create MemTable\n");
        return false;
    }
    
    // 插入键值对
    const char* key = "test_key";
    const char* value = "test_value";
    if (!memtable_put(table, key, strlen(key), value, strlen(value) + 1)) {
        printf("Failed to put key-value pair\n");
        memtable_destroy(table);
        return false;
    }
    
    // 删除键值对
    if (!memtable_delete(table, key, strlen(key))) {
        printf("Failed to delete key-value pair\n");
        memtable_destroy(table);
        return false;
    }
    
    // 验证键值对已删除
    void* found_value;
    uint32_t found_len;
    if (memtable_get(table, key, strlen(key), &found_value, &found_len)) {
        printf("Key still exists after deletion\n");
        memtable_destroy(table);
        return false;
    }
    
    // 销毁MemTable
    memtable_destroy(table);
    return true;
}

// 测试MemTable大小限制
static bool test_size_limit() {
    printf("Testing MemTable size limit...\n");
    
    // 创建MemTable
    memtable_t* table = memtable_create();
    if (!table) {
        printf("Failed to create MemTable\n");
        return false;
    }
    
    // 尝试插入大量数据
    char key[32];
    char value[32];
    for (uint32_t i = 0; i < 1000; i++) {
        sprintf(key, "key%u", i);
        sprintf(value, "value%u", i);
        memtable_put(table, key, strlen(key), value, strlen(value) + 1);
    }
    
    // 验证大小限制
    if (memtable_size(table) > MEMTABLE_MAX_SIZE) {
        printf("MemTable size exceeds limit\n");
        memtable_destroy(table);
        return false;
    }
    
    // 销毁MemTable
    memtable_destroy(table);
    return true;
}

// 测试MemTable迭代器
static bool test_iterator() {
    printf("Testing MemTable iterator...\n");
    
    // 创建MemTable
    memtable_t* table = memtable_create();
    if (!table) {
        printf("Failed to create MemTable\n");
        return false;
    }
    
    // 插入有序键值对
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    for (uint32_t i = 0; i < 3; i++) {
        if (!memtable_put(table, keys[i], strlen(keys[i]), values[i], strlen(values[i]) + 1)) {
            printf("Failed to put key-value pair\n");
            memtable_destroy(table);
            return false;
        }
    }
    
    // 创建迭代器
    memtable_iter_t* iter = memtable_iter_create(table);
    if (!iter) {
        printf("Failed to create iterator\n");
        memtable_destroy(table);
        return false;
    }
    
    // 验证迭代顺序
    uint32_t count = 0;
    while (memtable_iter_valid(iter)) {
        const char* key = memtable_iter_key(iter);
        const char* value = memtable_iter_value(iter);
        
        if (strcmp(key, keys[count]) != 0 || strcmp(value, values[count]) != 0) {
            printf("Iterator key-value mismatch\n");
            memtable_iter_destroy(iter);
            memtable_destroy(table);
            return false;
        }
        
        count++;
        memtable_iter_next(iter);
    }
    
    // 验证迭代完整性
    if (count != 3) {
        printf("Iterator count mismatch\n");
        memtable_iter_destroy(iter);
        memtable_destroy(table);
        return false;
    }
    
    // 销毁迭代器和MemTable
    memtable_iter_destroy(iter);
    memtable_destroy(table);
    return true;
}

// MemTable测试用例
static test_case_t memtable_test_cases[] = {
    {"create_destroy", test_create_destroy},
    {"basic_ops", test_basic_ops},
    {"delete", test_delete},
    {"size_limit", test_size_limit},
    {"iterator", test_iterator}
};

// MemTable测试套件
static test_suite_t memtable_test_suite = {
    "MemTable",
    memtable_test_cases,
    sizeof(memtable_test_cases) / sizeof(memtable_test_cases[0])
};

int main() {
    printf("Starting MemTable tests...\n");
    
    // 初始化日志系统
    ppdb_log_init(NULL);
    
    // 运行测试套件
    int result = run_test_suite(&memtable_test_suite);
    
    printf("MemTable tests completed with result: %d\n", result);
    return result;
} 