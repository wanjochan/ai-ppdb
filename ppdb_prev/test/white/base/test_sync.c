#include <cosmopolitan.h>
#include "internal/base.h"
#include "../test_macros.h"

// 线程函数声明
static void thread_func(void* arg);

// 测试同步原语的基本功能
int test_sync_basic(void) {
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
int test_sync_lockfree(void) {
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
int test_sync_concurrent(void) {
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
    
    ppdb_base_thread_t* threads[NUM_THREADS];
    
    // 启动线程
    for (int i = 0; i < NUM_THREADS; i++) {
        ppdb_error_t err = ppdb_base_thread_create(&threads[i], thread_func, sync);
        if (err != PPDB_OK) {
            printf("Thread creation error: %d\n", err);
            return err;
        }
    }
    
    // 等待线程完成并检查运行时间
    uint64_t total_wall_time = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        void* retval;
        ppdb_error_t err = ppdb_base_thread_join(threads[i], &retval);
        if (err != PPDB_OK) {
            printf("Thread join error: %d\n", err);
            return err;
        }
        
        // 获取线程运行时间
        uint64_t wall_time = ppdb_base_thread_get_wall_time(threads[i]);
        total_wall_time += wall_time;
        printf("Thread %d wall time: %lu us\n", i, wall_time);
        
        // 检查线程状态
        int state = ppdb_base_thread_get_state(threads[i]);
        printf("Thread %d final state: %d\n", i, state);
        
        ppdb_base_thread_destroy(threads[i]);
    }
    
    printf("Average thread wall time: %lu us\n", total_wall_time / NUM_THREADS);
    
    // 清理
    ASSERT_OK(ppdb_base_sync_destroy(sync));
    return 0;
}

// 测试错误处理
int test_sync_errors(void) {
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
    
    // 测试线程相关错误
    ppdb_base_thread_t* thread = NULL;
    ASSERT_ERR(ppdb_base_thread_create(NULL, thread_func, NULL), PPDB_BASE_ERR_PARAM);
    ASSERT_ERR(ppdb_base_thread_create(&thread, NULL, NULL), PPDB_BASE_ERR_PARAM);
    ASSERT_ERR(ppdb_base_thread_join(NULL, NULL), PPDB_BASE_ERR_PARAM);
    
    return 0;
}

// 线程函数实现
static void thread_func(void* arg) {
    ppdb_base_sync_t* sync = (ppdb_base_sync_t*)arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        ppdb_error_t err = ppdb_base_sync_lock(sync);
        if (err != PPDB_OK) {
            printf("Lock error: %d (%s)\n", err, ppdb_base_thread_get_error(NULL));
            return;
        }
        // 模拟临界区操作
        usleep(1);
        err = ppdb_base_sync_unlock(sync);
        if (err != PPDB_OK) {
            printf("Unlock error: %d (%s)\n", err, ppdb_base_thread_get_error(NULL));
            return;
        }
    }
}

// 条件变量测试
static int test_cond_var(void) {
    printf("\n=== Running condition variable tests ===\n");
    
    ppdb_base_mutex_t* mutex = NULL;
    ppdb_base_cond_t* cond = NULL;
    
    ASSERT_OK(ppdb_base_mutex_create(&mutex));
    ASSERT_OK(ppdb_base_cond_create(&cond));
    
    // 测试等待和通知
    ASSERT_OK(ppdb_base_mutex_lock(mutex));
    ASSERT_OK(ppdb_base_cond_timedwait(cond, mutex, 1000000)); // 1秒超时
    ASSERT_OK(ppdb_base_mutex_unlock(mutex));
    
    ASSERT_OK(ppdb_base_cond_signal(cond));
    ASSERT_OK(ppdb_base_cond_broadcast(cond));
    
    ASSERT_OK(ppdb_base_cond_destroy(cond));
    ASSERT_OK(ppdb_base_mutex_destroy(mutex));
    
    return 0;
}

// 条件变量并发测试
static void* cond_wait_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    ppdb_base_mutex_lock(data->mutex);
    while (!data->ready) {
        ppdb_base_cond_wait(data->cond, data->mutex);
    }
    ppdb_base_mutex_unlock(data->mutex);
    return NULL;
}

static int test_cond_concurrent(void) {
    printf("\n=== Running concurrent condition variable tests ===\n");
    
    thread_data_t data = {0};
    ASSERT_OK(ppdb_base_mutex_create(&data.mutex));
    ASSERT_OK(ppdb_base_cond_create(&data.cond));
    
    #define NUM_WAITERS 4
    ppdb_base_thread_t* threads[NUM_WAITERS];
    
    // 创建等待线程
    for (int i = 0; i < NUM_WAITERS; i++) {
        ASSERT_OK(ppdb_base_thread_create(&threads[i], cond_wait_thread, &data));
    }
    
    // 等待一会儿让线程进入等待状态
    usleep(100000);
    
    // 唤醒所有线程
    ppdb_base_mutex_lock(data.mutex);
    data.ready = true;
    ppdb_base_cond_broadcast(data.cond);
    ppdb_base_mutex_unlock(data.mutex);
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_WAITERS; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i]));
    }
    
    ASSERT_OK(ppdb_base_cond_destroy(data.cond));
    ASSERT_OK(ppdb_base_mutex_destroy(data.mutex));
    
    return 0;
}

int main(void) {
    TEST_CASE(test_sync_basic);
    TEST_CASE(test_sync_lockfree);
    TEST_CASE(test_sync_concurrent);
    TEST_CASE(test_sync_errors);
    TEST_CASE(test_cond_var);
    TEST_CASE(test_cond_concurrent);
    
    printf("\nTest summary:\n");
    printf("  Total: %d\n", g_test_count);
    printf("  Passed: %d\n", g_test_passed);
    printf("  Failed: %d\n", g_test_failed);
    
    return g_test_failed > 0 ? 1 : 0;
} 