#include "ppdb/internal.h"
#include "test_plan.h"

// 测试同步原语的基本功能
int test_core_sync_basic(void) {
    ppdb_core_sync_t* sync = NULL;
    ppdb_core_sync_config_t config = {
        .use_lockfree = false,
        .collect_stats = true,
        .backoff_us = 1,
        .max_backoff_us = 1000,
        .max_retries = 1000
    };
    
    // 测试创建
    ASSERT_OK(ppdb_core_sync_create(&sync, &config));
    ASSERT_NOT_NULL(sync);
    ASSERT_NOT_NULL(sync->stats);
    
    // 测试加锁解锁
    ASSERT_OK(ppdb_core_sync_lock(sync));
    ASSERT_OK(ppdb_core_sync_unlock(sync));
    
    // 测试统计信息
    ASSERT_TRUE(sync->stats->contention_count > 0);
    ASSERT_TRUE(sync->stats->total_wait_time_us >= 0);
    
    // 测试销毁
    ASSERT_OK(ppdb_core_sync_destroy(sync));
    return 0;
}

// 测试无锁模式
int test_core_sync_lockfree(void) {
    ppdb_core_sync_t* sync = NULL;
    ppdb_core_sync_config_t config = {
        .use_lockfree = true,
        .collect_stats = true,
        .backoff_us = 1,
        .max_backoff_us = 1000,
        .max_retries = 1000
    };
    
    // 测试创建
    ASSERT_OK(ppdb_core_sync_create(&sync, &config));
    ASSERT_NOT_NULL(sync);
    
    // 测试加锁解锁
    ASSERT_OK(ppdb_core_sync_lock(sync));
    ASSERT_OK(ppdb_core_sync_unlock(sync));
    
    // 测试版本号
    ASSERT_TRUE(atomic_load(&sync->version) > 0);
    
    // 测试销毁
    ASSERT_OK(ppdb_core_sync_destroy(sync));
    return 0;
}

// 测试并发性能
int test_core_sync_concurrent(void) {
    ppdb_core_sync_t* sync = NULL;
    ppdb_core_sync_config_t config = {
        .use_lockfree = true,
        .collect_stats = true,
        .backoff_us = 1,
        .max_backoff_us = 1000,
        .max_retries = 1000
    };
    
    // 创建同步原语
    ASSERT_OK(ppdb_core_sync_create(&sync, &config));
    
    // 创建多个线程进行并发测试
    #define NUM_THREADS 8
    #define OPS_PER_THREAD 1000
    
    pthread_t threads[NUM_THREADS];
    
    // 线程函数
    void* thread_func(void* arg) {
        ppdb_core_sync_t* sync = (ppdb_core_sync_t*)arg;
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            ASSERT_OK(ppdb_core_sync_lock(sync));
            // 模拟临界区操作
            usleep(1);
            ASSERT_OK(ppdb_core_sync_unlock(sync));
        }
        return NULL;
    }
    
    // 启动线程
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_func, sync);
    }
    
    // 等待线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 验证统计信息
    ASSERT_TRUE(sync->stats->contention_count > 0);
    ASSERT_TRUE(sync->stats->retry_count > 0);
    ASSERT_TRUE(sync->stats->total_wait_time_us > 0);
    
    // 清理
    ASSERT_OK(ppdb_core_sync_destroy(sync));
    return 0;
}

// 测试超时处理
int test_core_sync_timeout(void) {
    ppdb_core_sync_t* sync = NULL;
    ppdb_core_sync_config_t config = {
        .use_lockfree = true,
        .collect_stats = true,
        .backoff_us = 1,
        .max_backoff_us = 10,
        .max_retries = 10  // 设置较小的重试次数以触发超时
    };
    
    // 创建同步原语
    ASSERT_OK(ppdb_core_sync_create(&sync, &config));
    
    // 先获取锁
    ASSERT_OK(ppdb_core_sync_lock(sync));
    
    // 创建新线程尝试获取锁
    pthread_t thread;
    void* thread_func(void* arg) {
        ppdb_core_sync_t* sync = (ppdb_core_sync_t*)arg;
        ppdb_error_t err = ppdb_core_sync_lock(sync);
        ASSERT_TRUE(err == PPDB_ERR_TIMEOUT);
        return NULL;
    }
    
    pthread_create(&thread, NULL, thread_func, sync);
    pthread_join(thread, NULL);
    
    // 验证统计信息
    ASSERT_TRUE(sync->stats->timeout_count > 0);
    
    // 解锁并清理
    ASSERT_OK(ppdb_core_sync_unlock(sync));
    ASSERT_OK(ppdb_core_sync_destroy(sync));
    return 0;
}

// 测试错误处理
int test_core_sync_errors(void) {
    ppdb_core_sync_t* sync = NULL;
    ppdb_core_sync_config_t config = {
        .use_lockfree = false,
        .collect_stats = true
    };
    
    // 测试空指针
    ASSERT_ERR(ppdb_core_sync_create(NULL, &config), PPDB_ERR_NULL_POINTER);
    ASSERT_ERR(ppdb_core_sync_create(&sync, NULL), PPDB_ERR_NULL_POINTER);
    
    // 测试未初始化的同步原语
    ASSERT_ERR(ppdb_core_sync_lock(NULL), PPDB_ERR_NULL_POINTER);
    ASSERT_ERR(ppdb_core_sync_unlock(NULL), PPDB_ERR_NULL_POINTER);
    ASSERT_ERR(ppdb_core_sync_destroy(NULL), PPDB_ERR_NULL_POINTER);
    
    return 0;
}

int main(void) {
    TEST_CASE(test_core_sync_basic);
    TEST_CASE(test_core_sync_lockfree);
    TEST_CASE(test_core_sync_concurrent);
    TEST_CASE(test_core_sync_timeout);
    TEST_CASE(test_core_sync_errors);
    return 0;
}
