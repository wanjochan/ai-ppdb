#include <cosmopolitan.h>
#include "internal/base.h"
#include "../test_macros.h"
#include "test_counter.h"

#define NUM_THREADS 4
#define NUM_ITERATIONS 1000

static void counter_thread_func(void* arg);

int test_counter_basic(void) {
    ppdb_base_counter_t* counter = NULL;
    uint64_t value = 0;

    // 测试创建
    ASSERT_OK(ppdb_base_counter_create(&counter, "test_counter"));
    ASSERT_NOT_NULL(counter);

    // 测试基本操作
    ASSERT_OK(ppdb_base_counter_increment(counter));
    ASSERT_OK(ppdb_base_counter_get(counter, &value));
    ASSERT_EQ(value, 1);

    ASSERT_OK(ppdb_base_counter_decrement(counter));
    ASSERT_OK(ppdb_base_counter_get(counter, &value));
    ASSERT_EQ(value, 0);

    // 测试销毁
    ASSERT_OK(ppdb_base_counter_destroy(counter));
    return 0;
}

int test_counter_concurrent(void) {
    ppdb_base_counter_t* counter = NULL;
    ppdb_base_thread_t* threads[NUM_THREADS];
    uint64_t value = 0;

    // 创建计数器
    ASSERT_OK(ppdb_base_counter_create(&counter, "test_counter_concurrent"));

    // 创建多个线程并发操作
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_OK(ppdb_base_thread_create(&threads[i], counter_thread_func, counter));
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i]));
        ASSERT_OK(ppdb_base_thread_destroy(threads[i]));
    }

    // 验证最终结果
    ASSERT_OK(ppdb_base_counter_get(counter, &value));
    ASSERT_EQ(value, NUM_THREADS * NUM_ITERATIONS);

    ASSERT_OK(ppdb_base_counter_destroy(counter));
    return 0;
}

int test_counter_errors(void) {
    ppdb_base_counter_t* counter = NULL;
    uint64_t value = 0;

    // 测试空参数
    ASSERT_ERROR(ppdb_base_counter_create(NULL, "test_counter"));
    ASSERT_ERROR(ppdb_base_counter_create(&counter, NULL));

    // 测试无效操作
    ASSERT_ERROR(ppdb_base_counter_increment(NULL));
    ASSERT_ERROR(ppdb_base_counter_decrement(NULL));
    ASSERT_ERROR(ppdb_base_counter_get(NULL, &value));
    ASSERT_ERROR(ppdb_base_counter_get(counter, NULL));

    return 0;
}

static void counter_thread_func(void* arg) {
    ppdb_base_counter_t* counter = (ppdb_base_counter_t*)arg;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        ppdb_base_counter_increment(counter);
    }
}

int main(void) {
    TEST_RUN(test_counter_basic);
    TEST_RUN(test_counter_concurrent);
    TEST_RUN(test_counter_errors);
    return 0;
} 