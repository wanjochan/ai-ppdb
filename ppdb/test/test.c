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

// 测试用例列表
typedef struct {
    const char* name;
    void (*fn)(void);
} test_case_t;

static test_case_t test_cases[] = {
    {"sync", test_sync_unified},
    {"skiplist", test_skiplist_unified},
    {"memtable", test_memtable_unified},
    {"wal", test_wal_unified},
    {"performance", test_performance},
    {NULL, NULL}  // 结束标记
};

// 打印帮助信息
static void print_usage(void) {
    printf("Usage: test [options] [test_name]\n");
    printf("Options:\n");
    printf("  --list     List all available tests\n");
    printf("  --all      Run all tests\n");
    printf("  --help     Show this help message\n");
    printf("\nAvailable tests:\n");
    for (test_case_t* test = test_cases; test->name != NULL; test++) {
        printf("  %s\n", test->name);
    }
}

int main(int argc, char* argv[]) {
    // 初始化日志
    ppdb_log_config_t log_config = {
        .enabled = true,
        .outputs = PPDB_LOG_CONSOLE,
        .types = PPDB_LOG_TYPE_ALL,
        .async_mode = false,
        .buffer_size = 4096,
        .log_file = NULL,
        .level = PPDB_LOG_DEBUG
    };
    ppdb_log_init(&log_config);

    // 如果没有参数,显示帮助信息
    if (argc < 2) {
        print_usage();
        return 1;
    }

    // 处理命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        }
        else if (strcmp(argv[i], "--list") == 0) {
            printf("Available tests:\n");
            for (test_case_t* test = test_cases; test->name != NULL; test++) {
                printf("  %s\n", test->name);
            }
            return 0;
        }
        else if (strcmp(argv[i], "--all") == 0) {
            // 运行所有测试
            int failed = 0;
            for (test_case_t* test = test_cases; test->name != NULL; test++) {
                printf("\nRunning test: %s\n", test->name);
                test->fn();
                if (ppdb_test_get_failed_count() > failed) {
                    failed = ppdb_test_get_failed_count();
                    printf("Test %s failed\n", test->name);
                } else {
                    printf("Test %s passed\n", test->name);
                }
            }
            return failed;
        }
        else {
            // 查找并运行指定的测试
            bool found = false;
            for (test_case_t* test = test_cases; test->name != NULL; test++) {
                if (strcmp(argv[i], test->name) == 0) {
                    printf("\nRunning test: %s\n", test->name);
                    test->fn();
                    found = true;
                    break;
                }
            }
            if (!found) {
                printf("Error: Unknown test '%s'\n", argv[i]);
                print_usage();
                return 1;
            }
        }
    }

    int failed = ppdb_test_get_failed_count();
    if (failed > 0) {
        printf("\n%d test(s) failed\n", failed);
    } else {
        printf("\nAll tests passed\n");
    }

    ppdb_log_shutdown();
    return failed;
}
