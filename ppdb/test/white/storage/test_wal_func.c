#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "test/test_utils.h"

// 测试段管理
void test_segment_management(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 写入数据直到创建多个段
    const char* key = "test_key";
    const char* value = "test_value";
    for (int i = 0; i < 200; i++) {
        ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));
    }

    // 验证段数量不超过最大值
    ASSERT_LE(wal->segment_count, config.max_segments);

    // 验证段大小不超过限制
    wal_segment_t* curr = wal->segments;
    while (curr) {
        ASSERT_LE(curr->size, config.segment_size);
        curr = curr->next;
    }

    // 清理
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

// 测试写入缓冲区
void test_write_buffer(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = false  // 关闭同步写入以测试缓冲
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 写入小于缓冲区的数据
    const char* key = "key";
    const char* value = "value";
    ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));

    // 验证数据在缓冲区中
    ASSERT_GT(wal->buffer_used, 0);
    ASSERT_LT(wal->buffer_used, WAL_BUFFER_SIZE);

    // 写入直到缓冲区满
    for (int i = 0; i < 100; i++) {
        ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));
    }

    // 验证缓冲区已刷新
    ASSERT_EQ(wal->buffer_used, 0);

    // 清理
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

// 测试基本恢复
void test_basic_recovery(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 写入一些数据
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    
    for (int i = 0; i < 3; i++) {
        ASSERT_OK(ppdb_wal_write(wal, keys[i], strlen(keys[i]), 
                                values[i], strlen(values[i])));
    }

    // 创建内存表
    ppdb_memtable_t* memtable = NULL;
    ASSERT_OK(ppdb_memtable_create(&memtable));

    // 恢复数据到内存表
    ASSERT_OK(ppdb_wal_recover(wal, memtable));

    // 验证恢复的数据
    for (int i = 0; i < 3; i++) {
        void* value = NULL;
        size_t value_size = 0;
        ASSERT_OK(ppdb_memtable_get(memtable, keys[i], strlen(keys[i]), 
                                   &value, &value_size));
        ASSERT_EQ(value_size, strlen(values[i]));
        ASSERT_EQ(memcmp(value, values[i], value_size), 0);
        free(value);
    }

    // 清理
    ppdb_memtable_destroy(memtable);
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

// 测试基本迭代
void test_basic_iterator(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 写入一些数据
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    
    for (int i = 0; i < 3; i++) {
        ASSERT_OK(ppdb_wal_write(wal, keys[i], strlen(keys[i]), 
                                values[i], strlen(values[i])));
    }

    // 创建迭代器
    ppdb_wal_iterator_t* iter = NULL;
    ASSERT_OK(ppdb_wal_iterator_create(wal, &iter));

    // 遍历并验证数据
    int count = 0;
    while (ppdb_wal_iterator_valid(iter)) {
        void* key = NULL;
        void* value = NULL;
        size_t key_size = 0;
        size_t value_size = 0;

        ASSERT_OK(ppdb_wal_iterator_get(iter, &key, &key_size, &value, &value_size));
        
        ASSERT_EQ(key_size, strlen(keys[count]));
        ASSERT_EQ(value_size, strlen(values[count]));
        ASSERT_EQ(memcmp(key, keys[count], key_size), 0);
        ASSERT_EQ(memcmp(value, values[count], value_size), 0);

        free(key);
        free(value);
        count++;

        ASSERT_OK(ppdb_wal_iterator_next(iter));
    }

    ASSERT_EQ(count, 3);

    // 清理
    ppdb_wal_iterator_destroy(iter);
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

int main(void) {
    TEST_INIT();
    
    RUN_TEST(test_segment_management);
    RUN_TEST(test_write_buffer);
    RUN_TEST(test_basic_recovery);
    RUN_TEST(test_basic_iterator);
    
    TEST_SUMMARY();
    return 0;
} 