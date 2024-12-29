#include <cosmopolitan.h>
#include "ppdb/memtable.h"

// 测试创建和销毁
static void test_create_destroy(void) {
    printf("Testing MemTable Create/Destroy...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    printf("  Create MemTable (size=1024): %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);
    assert(table != NULL);
    
    size_t size = ppdb_memtable_size(table);
    printf("  Initial size: %zu\n", size);
    assert(size == 0);
    
    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// 测试基本的Put/Get操作
static void test_put_get(void) {
    printf("Testing MemTable Put/Get...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 测试数据
    const uint8_t key[] = "test_key";
    const uint8_t value[] = "test_value";
    
    // Put操作
    err = ppdb_memtable_put(table, key, sizeof(key), value, sizeof(value));
    printf("  Put [key='%s', value='%s']: %s\n", key, value, err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);
    
    size_t current_size = ppdb_memtable_size(table);
    printf("  Current size: %zu\n", current_size);
    assert(current_size > 0);

    // Get操作
    uint8_t buf[256];
    size_t buf_len = sizeof(buf);
    err = ppdb_memtable_get(table, key, sizeof(key), buf, &buf_len);
    printf("  Get [key='%s']: %s\n", key, err == PPDB_OK ? "OK" : "Failed");
    if (err == PPDB_OK) {
        printf("  Retrieved value: '%s' (length: %zu)\n", buf, buf_len);
    }
    assert(err == PPDB_OK);
    assert(buf_len == sizeof(value));
    assert(memcmp(buf, value, buf_len) == 0);

    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// 测试删除操作
static void test_delete(void) {
    printf("Testing MemTable Delete...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 插入数据
    const uint8_t key[] = "test_key";
    const uint8_t value[] = "test_value";
    err = ppdb_memtable_put(table, key, sizeof(key), value, sizeof(value));
    printf("  Put [key='%s', value='%s']: %s\n", key, value, err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 删除数据
    err = ppdb_memtable_delete(table, key, sizeof(key));
    printf("  Delete [key='%s']: %s\n", key, err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 验证删除
    uint8_t buf[256];
    size_t buf_len = sizeof(buf);
    err = ppdb_memtable_get(table, key, sizeof(key), buf, &buf_len);
    printf("  Verify delete [key='%s']: %s\n", key, err == PPDB_ERR_NOT_FOUND ? "OK" : "Failed");
    assert(err == PPDB_ERR_NOT_FOUND);

    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// 测试大小限制
static void test_size_limit(void) {
    printf("Testing MemTable Size Limit...\n");
    
    ppdb_memtable_t* table = NULL;
    // 设置一个较小的大小限制
    size_t max_size = 32;
    ppdb_error_t err = ppdb_memtable_create(max_size, &table);
    printf("  Create MemTable (max_size=%zu): %s\n", max_size, err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 尝试插入超过限制的数据
    const uint8_t key[] = "test_key";
    const uint8_t value[] = "this_is_a_very_long_value_that_exceeds_the_limit";
    printf("  Try to put large data [key='%s', value='%s' (length: %zu)]\n", key, value, sizeof(value));
    err = ppdb_memtable_put(table, key, sizeof(key), value, sizeof(value));
    printf("  Result: %s\n", err == PPDB_ERR_OUT_OF_MEMORY ? "Correctly rejected" : "Incorrectly accepted");
    assert(err == PPDB_ERR_OUT_OF_MEMORY);

    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// 测试更新操作
static void test_update(void) {
    printf("Testing MemTable Update...\n");
    
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 插入初始数据
    const uint8_t key[] = "test_key";
    const uint8_t value1[] = "value1";
    err = ppdb_memtable_put(table, key, sizeof(key), value1, sizeof(value1));
    printf("  Put [key='%s', value='%s']: %s\n", key, value1, err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 更新数据
    const uint8_t value2[] = "value2";
    err = ppdb_memtable_put(table, key, sizeof(key), value2, sizeof(value2));
    printf("  Update [key='%s', new_value='%s']: %s\n", key, value2, err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 验证更新
    uint8_t buf[256];
    size_t buf_len = sizeof(buf);
    err = ppdb_memtable_get(table, key, sizeof(key), buf, &buf_len);
    printf("  Verify update [key='%s']: %s\n", key, err == PPDB_OK ? "OK" : "Failed");
    if (err == PPDB_OK) {
        printf("  Retrieved value: '%s' (length: %zu)\n", buf, buf_len);
    }
    assert(err == PPDB_OK);
    assert(buf_len == sizeof(value2));
    assert(memcmp(buf, value2, buf_len) == 0);

    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

int main(int argc, char* argv[]) {
    printf("Starting MemTable Basic Tests...\n\n");
    
    // 运行所有测试
    test_create_destroy();
    test_put_get();
    test_delete();
    test_size_limit();
    test_update();

    printf("All MemTable Basic Tests passed!\n");
    return 0;
} 