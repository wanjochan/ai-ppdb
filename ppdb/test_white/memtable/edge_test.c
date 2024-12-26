#include <cosmopolitan.h>
#include "ppdb/memtable.h"

// 测试空键
static void test_empty_key(void) {
    printf("Testing Empty Key...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 尝试插入空键
    const uint8_t key[] = "";
    const uint8_t value[] = "test_value";
    err = ppdb_memtable_put(table, key, 0, value, sizeof(value));
    printf("  Put empty key: %s\n", err == PPDB_ERR_INVALID_ARGUMENT ? "Correctly rejected" : "Incorrectly accepted");
    assert(err == PPDB_ERR_INVALID_ARGUMENT);

    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// 测试空值
static void test_empty_value(void) {
    printf("Testing Empty Value...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 插入空值
    const uint8_t key[] = "test_key";
    err = ppdb_memtable_put(table, key, sizeof(key), NULL, 0);
    printf("  Put empty value: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 验证空值
    uint8_t buf[256];
    size_t buf_len = sizeof(buf);
    err = ppdb_memtable_get(table, key, sizeof(key), buf, &buf_len);
    printf("  Get empty value: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);
    assert(buf_len == 0);

    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// 测试最大键长度
static void test_max_key_length(void) {
    printf("Testing Maximum Key Length...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024 * 1024, &table);  // 1MB
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 创建一个大键
    size_t key_size = 1024;  // 1KB 键
    uint8_t* key = malloc(key_size);
    assert(key != NULL);
    memset(key, 'K', key_size - 1);
    key[key_size - 1] = '\0';

    // 尝试插入大键
    const uint8_t value[] = "test_value";
    err = ppdb_memtable_put(table, key, key_size, value, sizeof(value));
    printf("  Put large key (size=%zu): %s\n", key_size, err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 验证大键
    uint8_t buf[1024];
    size_t buf_len = sizeof(buf);
    err = ppdb_memtable_get(table, key, key_size, buf, &buf_len);
    printf("  Get large key: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);
    assert(buf_len == sizeof(value));
    assert(memcmp(buf, value, buf_len) == 0);

    free(key);
    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// 测试最大值长度
static void test_max_value_length(void) {
    printf("Testing Maximum Value Length...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024 * 1024, &table);  // 1MB
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 创建一个大值
    size_t value_size = 100 * 1024;  // 100KB 值
    uint8_t* value = malloc(value_size);
    assert(value != NULL);
    memset(value, 'V', value_size - 1);
    value[value_size - 1] = '\0';

    // 尝试插入大值
    const uint8_t key[] = "test_key";
    err = ppdb_memtable_put(table, key, sizeof(key), value, value_size);
    printf("  Put large value (size=%zu): %s\n", value_size, err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 验证大值
    uint8_t* buf = malloc(value_size);
    assert(buf != NULL);
    size_t buf_len = value_size;
    err = ppdb_memtable_get(table, key, sizeof(key), buf, &buf_len);
    printf("  Get large value: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);
    assert(buf_len == value_size);
    assert(memcmp(buf, value, buf_len) == 0);

    free(value);
    free(buf);
    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

int main(int argc, char* argv[]) {
    printf("Starting MemTable Edge Case Tests...\n\n");
    
    // 运行所有测试
    test_empty_key();
    test_empty_value();
    test_max_key_length();
    test_max_value_length();

    printf("All MemTable Edge Case Tests passed!\n");
    return 0;
} 