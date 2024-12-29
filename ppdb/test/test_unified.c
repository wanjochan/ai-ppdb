#include "test_framework.h"
#include "ppdb/kvstore/common/sync.h"
#include "ppdb/kvstore/skiplist/skiplist.h"
#include "ppdb/kvstore/memtable/memtable.h"
#include "ppdb/kvstore/wal/wal.h"

// 同步机制测试
void test_sync_unified(void) {
    ppdb_sync_config_t config = {
        .use_lockfree = true,
        .stripe_count = 0,
        .spin_count = 1000,
        .backoff_us = 100
    };
    
    ppdb_sync_t sync;
    ppdb_sync_init(&sync, &config);
    
    // 测试基本加锁解锁
    TEST_ASSERT(ppdb_sync_try_lock(&sync));
    ppdb_sync_unlock(&sync);
    
    // 测试竞争
    bool locked = ppdb_sync_try_lock(&sync);
    TEST_ASSERT(locked);
    TEST_ASSERT(!ppdb_sync_try_lock(&sync));  // 应该失败
    ppdb_sync_unlock(&sync);
    
    ppdb_sync_destroy(&sync);
}

// 跳表测试
void test_skiplist_unified(void) {
    ppdb_skiplist_config_t config = {
        .sync_config = {
            .use_lockfree = true,
            .stripe_count = 4,
            .spin_count = 1000,
            .backoff_us = 100
        },
        .enable_hint = true,
        .max_size = 1024 * 1024,  // 1MB
        .max_level = 12
    };
    
    ppdb_skiplist_t* list = ppdb_skiplist_create(&config);
    TEST_ASSERT(list != NULL);
    
    // 测试插入
    const char* key = "test_key";
    const char* value = "test_value";
    int ret = ppdb_skiplist_insert(list, key, strlen(key),
                                 value, strlen(value));
    TEST_ASSERT(ret == PPDB_OK);
    
    // 测试查找
    void* found_value;
    size_t value_len;
    ret = ppdb_skiplist_find(list, key, strlen(key),
                           &found_value, &value_len);
    TEST_ASSERT(ret == PPDB_OK);
    TEST_ASSERT(value_len == strlen(value));
    TEST_ASSERT(memcmp(found_value, value, value_len) == 0);
    
    // 测试删除
    ret = ppdb_skiplist_remove(list, key, strlen(key));
    TEST_ASSERT(ret == PPDB_OK);
    
    // 测试迭代器
    ppdb_skiplist_iter_t* iter = ppdb_skiplist_iter_create(list);
    TEST_ASSERT(iter != NULL);
    ppdb_skiplist_iter_destroy(iter);
    
    ppdb_skiplist_destroy(list);
}

// MemTable测试
void test_memtable_unified(void) {
    ppdb_memtable_config_t config = {
        .sync_config = {
            .use_lockfree = true,
            .stripe_count = 4,
            .spin_count = 1000,
            .backoff_us = 100
        },
        .max_size = 1024 * 1024,  // 1MB
        .max_level = 12,
        .enable_compression = false,
        .enable_bloom_filter = true
    };
    
    ppdb_memtable_t* table = ppdb_memtable_create(&config);
    TEST_ASSERT(table != NULL);
    
    // 测试写入
    const char* key = "test_key";
    const char* value = "test_value";
    int ret = ppdb_memtable_put(table, key, strlen(key),
                               value, strlen(value));
    TEST_ASSERT(ret == PPDB_OK);
    
    // 测试读取
    void* found_value;
    size_t value_len;
    ret = ppdb_memtable_get(table, key, strlen(key),
                           &found_value, &value_len);
    TEST_ASSERT(ret == PPDB_OK);
    TEST_ASSERT(value_len == strlen(value));
    TEST_ASSERT(memcmp(found_value, value, value_len) == 0);
    
    // 测试删除
    ret = ppdb_memtable_delete(table, key, strlen(key));
    TEST_ASSERT(ret == PPDB_OK);
    
    // 测试不可变转换
    ppdb_memtable_make_immutable(table);
    TEST_ASSERT(ppdb_memtable_is_immutable(table));
    
    ppdb_memtable_destroy(table);
}

// WAL测试
void test_wal_unified(void) {
    ppdb_wal_config_t config = {
        .sync_config = {
            .use_lockfree = true,
            .stripe_count = 0,
            .spin_count = 1000,
            .backoff_us = 100
        },
        .buffer_size = 4096,
        .enable_group_commit = true,
        .group_commit_interval = 10,
        .enable_async_flush = false,
        .enable_checksum = true
    };
    
    const char* wal_file = "test_wal.log";
    ppdb_wal_t* wal = ppdb_wal_create(wal_file, &config);
    TEST_ASSERT(wal != NULL);
    
    // 测试写入
    const char* key = "test_key";
    const char* value = "test_value";
    int ret = ppdb_wal_append(wal, WAL_RECORD_PUT,
                            key, strlen(key),
                            value, strlen(value), 1);
    TEST_ASSERT(ret == PPDB_OK);
    
    // 测试同步
    ret = ppdb_wal_sync(wal);
    TEST_ASSERT(ret == PPDB_OK);
    
    // 测试恢复
    ppdb_wal_recovery_iter_t* iter = ppdb_wal_recovery_iter_create(wal);
    TEST_ASSERT(iter != NULL);
    
    if (ppdb_wal_recovery_iter_valid(iter)) {
        ppdb_wal_record_type_t type;
        void *found_key, *found_value;
        size_t key_size, value_size;
        uint64_t sequence;
        
        ret = ppdb_wal_recovery_iter_next(iter, &type,
                                        &found_key, &key_size,
                                        &found_value, &value_size,
                                        &sequence);
        TEST_ASSERT(ret == PPDB_OK);
        TEST_ASSERT(type == WAL_RECORD_PUT);
        TEST_ASSERT(key_size == strlen(key));
        TEST_ASSERT(value_size == strlen(value));
        TEST_ASSERT(sequence == 1);
    }
    
    ppdb_wal_recovery_iter_destroy(iter);
    ppdb_wal_destroy(wal);
    remove(wal_file);
}

// 性能测试
void test_performance(void) {
    ppdb_memtable_config_t config = {
        .sync_config = {
            .use_lockfree = true,
            .stripe_count = 8,
            .spin_count = 1000,
            .backoff_us = 100
        },
        .max_size = 10 * 1024 * 1024,  // 10MB
        .max_level = 12,
        .enable_compression = false,
        .enable_bloom_filter = true
    };
    
    ppdb_memtable_t* table = ppdb_memtable_create(&config);
    TEST_ASSERT(table != NULL);
    
    // 批量写入测试
    const int num_ops = 100000;
    char key[32], value[128];
    
    uint64_t start_time = get_current_time_us();
    
    for (int i = 0; i < num_ops; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        
        int ret = ppdb_memtable_put(table, key, strlen(key),
                                   value, strlen(value));
        TEST_ASSERT(ret == PPDB_OK);
    }
    
    uint64_t write_time = get_current_time_us() - start_time;
    printf("Write throughput: %.2f ops/s\n", 
           num_ops * 1000000.0 / write_time);
    
    // 批量读取测试
    start_time = get_current_time_us();
    void* found_value;
    size_t value_len;
    
    for (int i = 0; i < num_ops; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        int ret = ppdb_memtable_get(table, key, strlen(key),
                                   &found_value, &value_len);
        TEST_ASSERT(ret == PPDB_OK);
    }
    
    uint64_t read_time = get_current_time_us() - start_time;
    printf("Read throughput: %.2f ops/s\n",
           num_ops * 1000000.0 / read_time);
    
    ppdb_memtable_destroy(table);
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_sync_unified);
    TEST_RUN(test_skiplist_unified);
    TEST_RUN(test_memtable_unified);
    TEST_RUN(test_wal_unified);
    TEST_RUN(test_performance);
    
    TEST_REPORT();
    return TEST_EXIT_CODE();
}
