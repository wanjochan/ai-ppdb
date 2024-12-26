#include <cosmopolitan.h>
#include "ppdb/memtable.h"
#include "ppdb/skiplist.h"

// 测试基本迭代
static void test_basic_iteration(void) {
    printf("Testing Basic Iteration...\n");
    
    // 创建 MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 插入一些数据
    const char* keys[] = {"key1", "key2", "key3", "key4", "key5"};
    const char* values[] = {"value1", "value2", "value3", "value4", "value5"};
    int count = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < count; i++) {
        err = ppdb_memtable_put(table, (uint8_t*)keys[i], strlen(keys[i]) + 1,
                               (uint8_t*)values[i], strlen(values[i]) + 1);
        printf("  Put [key='%s', value='%s']: %s\n", keys[i], values[i], 
               err == PPDB_OK ? "OK" : "Failed");
        assert(err == PPDB_OK);
    }

    // 创建迭代器
    ppdb_skiplist_iterator_t* iter = NULL;
    err = ppdb_memtable_iterator_create(table, &iter);
    printf("  Create Iterator: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 遍历所有键值对
    printf("  Iterating through all key-value pairs:\n");
    int i = 0;
    while (ppdb_skiplist_iterator_valid(iter)) {
        const uint8_t* key;
        size_t key_len;
        const uint8_t* value;
        size_t value_len;

        err = ppdb_skiplist_iterator_key(iter, &key, &key_len);
        assert(err == PPDB_OK);
        err = ppdb_skiplist_iterator_value(iter, &value, &value_len);
        assert(err == PPDB_OK);

        printf("    [%d] key='%s', value='%s'\n", i++, key, value);
        ppdb_skiplist_iterator_next(iter);
    }

    ppdb_skiplist_iterator_destroy(iter);
    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// 测试定位迭代
static void test_seek_iteration(void) {
    printf("Testing Seek Iteration...\n");
    
    // 创建 MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 插入一些数据
    const char* keys[] = {"key10", "key20", "key30", "key40", "key50"};
    const char* values[] = {"value10", "value20", "value30", "value40", "value50"};
    int count = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < count; i++) {
        err = ppdb_memtable_put(table, (uint8_t*)keys[i], strlen(keys[i]) + 1,
                               (uint8_t*)values[i], strlen(values[i]) + 1);
        printf("  Put [key='%s', value='%s']: %s\n", keys[i], values[i], 
               err == PPDB_OK ? "OK" : "Failed");
        assert(err == PPDB_OK);
    }

    // 创建迭代器
    ppdb_skiplist_iterator_t* iter = NULL;
    err = ppdb_memtable_iterator_create(table, &iter);
    printf("  Create Iterator: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 测试 Seek
    const char* seek_key = "key25";
    printf("  Seeking to key '%s'...\n", seek_key);
    ppdb_skiplist_iterator_seek(iter, (uint8_t*)seek_key, strlen(seek_key) + 1);

    // 打印 Seek 后的位置
    if (ppdb_skiplist_iterator_valid(iter)) {
        const uint8_t* key;
        size_t key_len;
        const uint8_t* value;
        size_t value_len;

        err = ppdb_skiplist_iterator_key(iter, &key, &key_len);
        assert(err == PPDB_OK);
        err = ppdb_skiplist_iterator_value(iter, &value, &value_len);
        assert(err == PPDB_OK);

        printf("    Found position: key='%s', value='%s'\n", key, value);
    } else {
        printf("    Iterator reached end\n");
    }

    ppdb_skiplist_iterator_destroy(iter);
    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// 测试空表迭代
static void test_empty_iteration(void) {
    printf("Testing Empty Table Iteration...\n");
    
    // 创建 MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024, &table);
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 创建迭代器
    ppdb_skiplist_iterator_t* iter = NULL;
    err = ppdb_memtable_iterator_create(table, &iter);
    printf("  Create Iterator: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);

    // 验证迭代器无效
    printf("  Checking iterator validity: %s\n", 
           !ppdb_skiplist_iterator_valid(iter) ? "Correctly invalid" : "Incorrectly valid");
    assert(!ppdb_skiplist_iterator_valid(iter));

    ppdb_skiplist_iterator_destroy(iter);
    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

int main(int argc, char* argv[]) {
    printf("Starting MemTable Iterator Tests...\n\n");
    
    // 运行所有测试
    test_basic_iteration();
    test_seek_iteration();
    test_empty_iteration();
    
    printf("All MemTable Iterator Tests passed!\n");
    return 0;
} 