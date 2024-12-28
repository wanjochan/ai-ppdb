#include <cosmopolitan.h>
#include "../kvstore/ref_count.h"
#include "../kvstore/atomic_skiplist.h"
#include "../kvstore/sharded_memtable.h"
#include "ppdb/logger.h"

// 测试引用计数
static void test_ref_count() {
    printf("Testing reference counting...\n");
    
    // 创建测试数据
    int* data = malloc(sizeof(int));
    *data = 42;
    
    // 创建引用计数对象
    ref_count_t* ref = ref_count_create(data, free);
    assert(ref != NULL);
    assert(ref_count_get(ref) == 1);
    
    // 测试增加引用计数
    ref_count_inc(ref);
    assert(ref_count_get(ref) == 2);
    
    // 测试减少引用计数
    ref_count_dec(ref);
    assert(ref_count_get(ref) == 1);
    
    // 测试最后一次减少引用计数（应该触发销毁）
    ref_count_dec(ref);
    
    printf("Reference counting tests passed.\n");
}

// 测试无锁跳表的基本操作
static void test_atomic_skiplist() {
    printf("Testing atomic skiplist...\n");
    
    // 创建跳表
    atomic_skiplist_t* list = atomic_skiplist_create(16);
    assert(list != NULL);
    
    // 测试插入
    const char* key1 = "key1";
    const char* value1 = "value1";
    assert(atomic_skiplist_insert(list, key1, strlen(key1), value1, strlen(value1) + 1));
    
    // 测试重复插入
    assert(!atomic_skiplist_insert(list, key1, strlen(key1), "new_value", 9));
    
    // 测试查找
    void* found_value;
    uint32_t found_len;
    assert(atomic_skiplist_find(list, key1, strlen(key1), &found_value, &found_len));
    assert(strcmp(found_value, value1) == 0);
    
    // 测试不存在的键
    assert(!atomic_skiplist_find(list, "nonexistent", 10, &found_value, &found_len));
    
    // 测试删除
    assert(atomic_skiplist_delete(list, key1, strlen(key1)));
    assert(!atomic_skiplist_find(list, key1, strlen(key1), &found_value, &found_len));
    
    // 测试删除不存在的键
    assert(!atomic_skiplist_delete(list, "nonexistent", 10));
    
    // 测试大量数据
    char key[32];
    char value[32];
    for (uint32_t i = 0; i < 1000; i++) {
        sprintf(key, "key%u", i);
        sprintf(value, "value%u", i);
        assert(atomic_skiplist_insert(list, key, strlen(key), value, strlen(value) + 1));
    }
    assert(atomic_skiplist_size(list) == 1000);
    
    // 测试遍历
    atomic_skiplist_clear(list);
    assert(atomic_skiplist_size(list) == 0);
    
    // 清理
    atomic_skiplist_destroy(list);
    
    printf("Atomic skiplist tests passed.\n");
}

// 测试分片内存表的基本操作
static void test_sharded_memtable() {
    printf("Testing sharded memtable...\n");
    
    // 创建分片配置
    shard_config_t config = {
        .shard_bits = 4,
        .shard_count = 16,
        .max_size = 1000
    };
    
    // 创建分片内存表
    sharded_memtable_t* table = sharded_memtable_create(&config);
    assert(table != NULL);
    
    // 测试插入
    const char* key1 = "key1";
    const char* value1 = "value1";
    assert(sharded_memtable_put(table, key1, strlen(key1), value1, strlen(value1) + 1));
    
    // 测试重复插入
    assert(!sharded_memtable_put(table, key1, strlen(key1), "new_value", 9));
    
    // 测试查找
    void* found_value;
    uint32_t found_len;
    assert(sharded_memtable_get(table, key1, strlen(key1), &found_value, &found_len));
    assert(strcmp(found_value, value1) == 0);
    
    // 测试不存在的键
    assert(!sharded_memtable_get(table, "nonexistent", 10, &found_value, &found_len));
    
    // 测试删除
    assert(sharded_memtable_delete(table, key1, strlen(key1)));
    assert(!sharded_memtable_get(table, key1, strlen(key1), &found_value, &found_len));
    
    // 测试删除不存在的键
    assert(!sharded_memtable_delete(table, "nonexistent", 10));
    
    // 测试分片大小限制
    char key[32];
    char value[32];
    uint32_t count = 0;
    for (uint32_t i = 0; i < config.max_size * 2; i++) {
        sprintf(key, "key%u", i);
        sprintf(value, "value%u", i);
        if (sharded_memtable_put(table, key, strlen(key), value, strlen(value) + 1)) {
            count++;
        }
    }
    assert(count <= config.max_size * config.shard_count);
    
    // 测试分片大小
    uint32_t total_size = 0;
    for (uint32_t i = 0; i < config.shard_count; i++) {
        uint32_t shard_size = sharded_memtable_shard_size(table, i);
        assert(shard_size <= config.max_size);
        total_size += shard_size;
    }
    assert(total_size == sharded_memtable_size(table));
    
    // 测试清空
    sharded_memtable_clear(table);
    assert(sharded_memtable_size(table) == 0);
    
    // 清理
    sharded_memtable_destroy(table);
    
    printf("Sharded memtable tests passed.\n");
}

// 并发测试参数
#define NUM_THREADS 8
#define NUM_OPERATIONS 10000

// 并发测试的线程函数
static void* concurrent_test_thread(void* arg) {
    sharded_memtable_t* table = (sharded_memtable_t*)arg;
    char key[32];
    char value[32];
    
    for (uint32_t i = 0; i < NUM_OPERATIONS; i++) {
        // 生成随机键值对
        sprintf(key, "key%u-%lu", i, (unsigned long)pthread_self());
        sprintf(value, "value%u", i);
        
        // 随机选择操作：插入、查找或删除
        int op = rand() % 3;
        switch (op) {
            case 0: // 插入
                sharded_memtable_put(table, key, strlen(key), value, strlen(value) + 1);
                break;
                
            case 1: { // 查找
                void* found_value;
                uint32_t found_len;
                sharded_memtable_get(table, key, strlen(key), &found_value, &found_len);
                break;
            }
                
            case 2: // 删除
                sharded_memtable_delete(table, key, strlen(key));
                break;
        }
    }
    
    return NULL;
}

// 并发测试
static void test_concurrent_operations() {
    printf("Testing concurrent operations...\n");
    
    // 创建分片配置
    shard_config_t config = {
        .shard_bits = 8,
        .shard_count = 256,
        .max_size = 10000
    };
    
    // 创建分片内存表
    sharded_memtable_t* table = sharded_memtable_create(&config);
    assert(table != NULL);
    
    // 创建线程
    pthread_t threads[NUM_THREADS];
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, concurrent_test_thread, table);
    }
    
    // 等待所有线程完成
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 验证最终状态
    assert(sharded_memtable_size(table) <= config.max_size * config.shard_count);
    for (uint32_t i = 0; i < config.shard_count; i++) {
        assert(sharded_memtable_shard_size(table, i) <= config.max_size);
    }
    
    // 清理
    sharded_memtable_destroy(table);
    
    printf("Concurrent operation tests passed.\n");
}

int main() {
    // 初始化日志系统
    ppdb_log_init(NULL);
    
    // 初始化随机数生成器
    srand(time(NULL));
    
    // 运行测试
    test_ref_count();
    test_atomic_skiplist();
    test_sharded_memtable();
    test_concurrent_operations();
    
    printf("All tests passed!\n");
    return 0;
} 