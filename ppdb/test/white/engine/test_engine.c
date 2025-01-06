#include "../../../src/internal/base.h"
#include "../../../src/internal/engine.h"
#include "../test_macros.h"

// 测试同步原语的基本功能
int test_engine_sync_basic(void) {
    ppdb_base_sync_t* sync = NULL;
    ppdb_base_sync_config_t config = {
        .thread_safe = true,
        .spin_count = 1000,
        .backoff_us = 1
    };
    
    // 测试创建
    ASSERT_OK(ppdb_base_sync_create(&sync, &config));
    ASSERT_NOT_NULL(sync);
    
    // 测试加锁解锁
    ASSERT_OK(ppdb_base_sync_lock(sync));
    ASSERT_OK(ppdb_base_sync_unlock(sync));
    
    // 测试销毁
    ASSERT_OK(ppdb_base_sync_destroy(sync));
    return 0;
}

// 测试无锁模式
int test_engine_sync_lockfree(void) {
    ppdb_base_sync_t* sync = NULL;
    ppdb_base_sync_config_t config = {
        .thread_safe = false,
        .spin_count = 1000,
        .backoff_us = 1
    };
    
    // 测试创建
    ASSERT_OK(ppdb_base_sync_create(&sync, &config));
    ASSERT_NOT_NULL(sync);
    
    // 测试加锁解锁
    ASSERT_OK(ppdb_base_sync_lock(sync));
    ASSERT_OK(ppdb_base_sync_unlock(sync));
    
    // 测试销毁
    ASSERT_OK(ppdb_base_sync_destroy(sync));
    return 0;
}

// 测试并发性能
int test_engine_sync_concurrent(void) {
    ppdb_base_sync_t* sync = NULL;
    ppdb_base_sync_config_t config = {
        .thread_safe = true,
        .spin_count = 1000,
        .backoff_us = 1
    };
    
    // 创建同步原语
    ASSERT_OK(ppdb_base_sync_create(&sync, &config));
    
    // 创建多个线程进行并发测试
    #define NUM_THREADS 8
    #define OPS_PER_THREAD 1000
    
    pthread_t threads[NUM_THREADS];
    
    // 线程函数
    void* thread_func(void* arg) {
        ppdb_base_sync_t* sync = (ppdb_base_sync_t*)arg;
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            ASSERT_OK(ppdb_base_sync_lock(sync));
            // 模拟临界区操作
            usleep(1);
            ASSERT_OK(ppdb_base_sync_unlock(sync));
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
    
    // 清理
    ASSERT_OK(ppdb_base_sync_destroy(sync));
    return 0;
}

// 测试错误处理
int test_engine_sync_errors(void) {
    ppdb_base_sync_t* sync = NULL;
    ppdb_base_sync_config_t config = {
        .thread_safe = true,
        .spin_count = 1000,
        .backoff_us = 1
    };
    
    // 测试空指针
    ASSERT_ERR(ppdb_base_sync_create(NULL, &config), PPDB_BASE_ERR_PARAM);
    ASSERT_ERR(ppdb_base_sync_create(&sync, NULL), PPDB_BASE_ERR_PARAM);
    
    // 测试未初始化的同步原语
    ASSERT_ERR(ppdb_base_sync_lock(NULL), PPDB_BASE_ERR_PARAM);
    ASSERT_ERR(ppdb_base_sync_unlock(NULL), PPDB_BASE_ERR_PARAM);
    ASSERT_ERR(ppdb_base_sync_destroy(NULL), PPDB_BASE_ERR_PARAM);
    
    return 0;
}

int main(void) {
    TEST_CASE(test_engine_sync_basic);
    TEST_CASE(test_engine_sync_lockfree);
    TEST_CASE(test_engine_sync_concurrent);
    TEST_CASE(test_engine_sync_errors);
    printf("\nTest summary:\n");
    printf("  Total: %d\n", g_test_count);
    printf("  Passed: %d\n", g_test_passed);
    printf("  Failed: %d\n", g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
