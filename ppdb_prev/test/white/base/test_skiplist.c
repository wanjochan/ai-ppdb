#include <cosmopolitan.h>
#include "internal/base.h"
#include "../test_macros.h"
#include "test_skiplist.h"

#define NUM_THREADS 4
#define NUM_OPERATIONS 1000
#define MAX_KEY_SIZE 32
#define MAX_VALUE_SIZE 128

static void skiplist_thread_func(void* arg);

typedef struct {
    ppdb_base_skiplist_t* list;
    int thread_id;
} thread_context_t;

// 基本功能测试
int test_skiplist_basic(void) {
    ppdb_base_skiplist_t* list = NULL;
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    void* found_value = NULL;
    size_t value_size = 0;

    // 测试初始化
    ASSERT_OK(ppdb_base_skiplist_init(list, PPDB_MAX_SKIPLIST_LEVEL));
    ASSERT_NOT_NULL(list);

    // 测试插入
    ASSERT_OK(ppdb_base_skiplist_insert(list, test_key, strlen(test_key), 
                                      test_value, strlen(test_value) + 1));

    // 测试查找
    ASSERT_OK(ppdb_base_skiplist_find(list, test_key, strlen(test_key), 
                                    &found_value, &value_size));
    ASSERT_NOT_NULL(found_value);
    ASSERT_EQ(strcmp(found_value, test_value), 0);

    // 测试更新
    const char* new_value = "updated_value";
    ASSERT_OK(ppdb_base_skiplist_insert(list, test_key, strlen(test_key),
                                      new_value, strlen(new_value) + 1));
    ASSERT_OK(ppdb_base_skiplist_find(list, test_key, strlen(test_key),
                                    &found_value, &value_size));
    ASSERT_EQ(strcmp(found_value, new_value), 0);

    // 测试大小
    size_t size = 0;
    ASSERT_OK(ppdb_base_skiplist_size(list, &size));
    ASSERT_EQ(size, 1);

    // 测试销毁
    ASSERT_OK(ppdb_base_skiplist_destroy(list));
    return 0;
}

// 并发测试
int test_skiplist_concurrent(void) {
    ppdb_base_skiplist_t* list = NULL;
    ppdb_base_thread_t* threads[NUM_THREADS];
    thread_context_t contexts[NUM_THREADS];

    // 初始化跳表
    ASSERT_OK(ppdb_base_skiplist_init(list, PPDB_MAX_SKIPLIST_LEVEL));

    // 创建多个线程并发操作
    for (int i = 0; i < NUM_THREADS; i++) {
        contexts[i].list = list;
        contexts[i].thread_id = i;
        ASSERT_OK(ppdb_base_thread_create(&threads[i], skiplist_thread_func, &contexts[i]));
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i]));
        ASSERT_OK(ppdb_base_thread_destroy(threads[i]));
    }

    // 验证结果
    size_t size = 0;
    ASSERT_OK(ppdb_base_skiplist_size(list, &size));
    ASSERT_EQ(size, NUM_THREADS * NUM_OPERATIONS);

    ASSERT_OK(ppdb_base_skiplist_destroy(list));
    return 0;
}

// 错误处理测试
int test_skiplist_errors(void) {
    ppdb_base_skiplist_t* list = NULL;
    void* value = NULL;
    size_t value_size = 0;

    // 测试空参数
    ASSERT_ERROR(ppdb_base_skiplist_init(NULL, PPDB_MAX_SKIPLIST_LEVEL));
    ASSERT_ERROR(ppdb_base_skiplist_destroy(NULL));
    ASSERT_ERROR(ppdb_base_skiplist_insert(NULL, "key", 3, "value", 5));
    ASSERT_ERROR(ppdb_base_skiplist_find(NULL, "key", 3, &value, &value_size));
    ASSERT_ERROR(ppdb_base_skiplist_size(NULL, &value_size));

    // 初始化跳表
    ASSERT_OK(ppdb_base_skiplist_init(list, PPDB_MAX_SKIPLIST_LEVEL));

    // 测试无效操作
    ASSERT_ERROR(ppdb_base_skiplist_insert(list, NULL, 0, "value", 5));
    ASSERT_ERROR(ppdb_base_skiplist_insert(list, "key", 3, NULL, 0));
    ASSERT_ERROR(ppdb_base_skiplist_find(list, NULL, 0, &value, &value_size));

    ASSERT_OK(ppdb_base_skiplist_destroy(list));
    return 0;
}

// 迭代器测试
int test_skiplist_iterator(void) {
    ppdb_base_skiplist_t* list = NULL;
    ppdb_base_skiplist_iterator_t* iterator = NULL;
    char key[32], value[32];
    void *found_key, *found_value;
    size_t key_size, value_size;
    bool valid = false;

    // 初始化跳表
    ASSERT_OK(ppdb_base_skiplist_init(list, PPDB_MAX_SKIPLIST_LEVEL));

    // 插入测试数据
    for (int i = 0; i < 10; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        ASSERT_OK(ppdb_base_skiplist_insert(list, key, strlen(key), 
                                          value, strlen(value) + 1));
    }

    // 创建迭代器
    ASSERT_OK(ppdb_base_skiplist_iterator_create(list, &iterator, false));
    ASSERT_NOT_NULL(iterator);

    // 正向遍历
    int count = 0;
    ASSERT_OK(ppdb_base_skiplist_iterator_valid(iterator, &valid));
    while (valid) {
        ASSERT_OK(ppdb_base_skiplist_iterator_key(iterator, &found_key, &key_size));
        ASSERT_OK(ppdb_base_skiplist_iterator_value(iterator, &found_value, &value_size));
        ASSERT_NOT_NULL(found_key);
        ASSERT_NOT_NULL(found_value);
        count++;
        ASSERT_OK(ppdb_base_skiplist_iterator_next(iterator));
        ASSERT_OK(ppdb_base_skiplist_iterator_valid(iterator, &valid));
    }
    ASSERT_EQ(count, 10);

    // 销毁迭代器和跳表
    ASSERT_OK(ppdb_base_skiplist_iterator_destroy(iterator));
    ASSERT_OK(ppdb_base_skiplist_destroy(list));
    return 0;
}

// 压力测试
int test_skiplist_stress(void) {
    ppdb_base_skiplist_t* list = NULL;
    char key[MAX_KEY_SIZE], value[MAX_VALUE_SIZE];
    void* found_value;
    size_t value_size;
    const int num_items = 10000;

    // 初始化跳表
    ASSERT_OK(ppdb_base_skiplist_init(list, PPDB_MAX_SKIPLIST_LEVEL));

    // 插入大量数据
    for (int i = 0; i < num_items; i++) {
        snprintf(key, sizeof(key), "stress_key_%d", i);
        snprintf(value, sizeof(value), "stress_value_%d", i);
        ASSERT_OK(ppdb_base_skiplist_insert(list, key, strlen(key),
                                          value, strlen(value) + 1));
    }

    // 随机查找
    for (int i = 0; i < num_items; i++) {
        int idx = rand() % num_items;
        snprintf(key, sizeof(key), "stress_key_%d", idx);
        ASSERT_OK(ppdb_base_skiplist_find(list, key, strlen(key),
                                        &found_value, &value_size));
        snprintf(value, sizeof(value), "stress_value_%d", idx);
        ASSERT_EQ(strcmp(found_value, value), 0);
    }

    // 验证大小
    size_t size = 0;
    ASSERT_OK(ppdb_base_skiplist_size(list, &size));
    ASSERT_EQ(size, num_items);

    ASSERT_OK(ppdb_base_skiplist_destroy(list));
    return 0;
}

// 删除操作测试
int test_skiplist_remove(void) {
    ppdb_base_skiplist_t* list = NULL;
    const char* test_keys[] = {"key1", "key2", "key3", "key4", "key5"};
    const char* test_values[] = {"value1", "value2", "value3", "value4", "value5"};
    void* found_value = NULL;
    size_t value_size = 0;
    size_t size = 0;

    // 初始化跳表
    ASSERT_OK(ppdb_base_skiplist_init(list, PPDB_MAX_SKIPLIST_LEVEL));
    ASSERT_NOT_NULL(list);

    // 插入测试数据
    for (int i = 0; i < 5; i++) {
        ASSERT_OK(ppdb_base_skiplist_insert(list, test_keys[i], strlen(test_keys[i]),
                                          test_values[i], strlen(test_values[i]) + 1));
    }

    // 验证初始大小
    ASSERT_OK(ppdb_base_skiplist_size(list, &size));
    ASSERT_EQ(size, 5);

    // 测试删除中间节点
    ASSERT_OK(ppdb_base_skiplist_remove(list, "key3", strlen("key3")));
    ASSERT_OK(ppdb_base_skiplist_size(list, &size));
    ASSERT_EQ(size, 4);
    ASSERT_ERROR(ppdb_base_skiplist_find(list, "key3", strlen("key3"),
                                       &found_value, &value_size));

    // 测试删除头节点
    ASSERT_OK(ppdb_base_skiplist_remove(list, "key1", strlen("key1")));
    ASSERT_OK(ppdb_base_skiplist_size(list, &size));
    ASSERT_EQ(size, 3);
    ASSERT_ERROR(ppdb_base_skiplist_find(list, "key1", strlen("key1"),
                                       &found_value, &value_size));

    // 测试删除尾节点
    ASSERT_OK(ppdb_base_skiplist_remove(list, "key5", strlen("key5")));
    ASSERT_OK(ppdb_base_skiplist_size(list, &size));
    ASSERT_EQ(size, 2);
    ASSERT_ERROR(ppdb_base_skiplist_find(list, "key5", strlen("key5"),
                                       &found_value, &value_size));

    // 测试删除不存在的节点
    ASSERT_ERROR(ppdb_base_skiplist_remove(list, "not_exist", strlen("not_exist")));
    ASSERT_OK(ppdb_base_skiplist_size(list, &size));
    ASSERT_EQ(size, 2);

    // 测试删除剩余节点
    ASSERT_OK(ppdb_base_skiplist_remove(list, "key2", strlen("key2")));
    ASSERT_OK(ppdb_base_skiplist_remove(list, "key4", strlen("key4")));
    ASSERT_OK(ppdb_base_skiplist_size(list, &size));
    ASSERT_EQ(size, 0);

    // 测试空跳表删除
    ASSERT_ERROR(ppdb_base_skiplist_remove(list, "key1", strlen("key1")));

    // 测试参数错误
    ASSERT_ERROR(ppdb_base_skiplist_remove(NULL, "key1", strlen("key1")));
    ASSERT_ERROR(ppdb_base_skiplist_remove(list, NULL, 0));

    ASSERT_OK(ppdb_base_skiplist_destroy(list));
    return 0;
}

// 删除操作并发测试
int test_skiplist_remove_concurrent(void) {
    ppdb_base_skiplist_t* list = NULL;
    ppdb_base_thread_t* threads[NUM_THREADS * 2];  // 一半线程插入，一半线程删除
    thread_context_t contexts[NUM_THREADS * 2];
    size_t size = 0;

    // 初始化跳表
    ASSERT_OK(ppdb_base_skiplist_init(list, PPDB_MAX_SKIPLIST_LEVEL));

    // 创建线程：一半插入，一半删除
    for (int i = 0; i < NUM_THREADS * 2; i++) {
        contexts[i].list = list;
        contexts[i].thread_id = i;
        if (i < NUM_THREADS) {
            // 插入线程
            ASSERT_OK(ppdb_base_thread_create(&threads[i], skiplist_thread_func, &contexts[i]));
        } else {
            // 删除线程
            ASSERT_OK(ppdb_base_thread_create(&threads[i], skiplist_remove_thread_func, &contexts[i]));
        }
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS * 2; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i]));
        ASSERT_OK(ppdb_base_thread_destroy(threads[i]));
    }

    // 验证最终大小（应该小于等于插入线程的总插入量）
    ASSERT_OK(ppdb_base_skiplist_size(list, &size));
    ASSERT_TRUE(size <= NUM_THREADS * NUM_OPERATIONS);

    ASSERT_OK(ppdb_base_skiplist_destroy(list));
    return 0;
}

static void skiplist_remove_thread_func(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    char key[MAX_KEY_SIZE];

    // 删除操作：尝试删除所有可能的键
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        snprintf(key, sizeof(key), "key_%d_%d", ctx->thread_id % NUM_THREADS, i);
        ppdb_base_skiplist_remove(ctx->list, key, strlen(key));
    }
}

static void skiplist_thread_func(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    char key[MAX_KEY_SIZE], value[MAX_VALUE_SIZE];

    for (int i = 0; i < NUM_OPERATIONS; i++) {
        snprintf(key, sizeof(key), "key_%d_%d", ctx->thread_id, i);
        snprintf(value, sizeof(value), "value_%d_%d", ctx->thread_id, i);
        ppdb_base_skiplist_insert(ctx->list, key, strlen(key),
                                value, strlen(value) + 1);
    }
}

int main(void) {
    TEST_RUN(test_skiplist_basic);
    TEST_RUN(test_skiplist_concurrent);
    TEST_RUN(test_skiplist_errors);
    TEST_RUN(test_skiplist_iterator);
    TEST_RUN(test_skiplist_stress);
    TEST_RUN(test_skiplist_remove);
    TEST_RUN(test_skiplist_remove_concurrent);
    return 0;
} 