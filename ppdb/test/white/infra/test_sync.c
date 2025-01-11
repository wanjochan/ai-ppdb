#include "internal/infra/infra_core.h"
#include "../framework/test_framework.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_platform.h"

static void* thread_func(void* arg) {
    int* counter = (int*)arg;
    (*counter)++;
    return NULL;
}

static void test_thread(void) {
    infra_error_t err;
    infra_thread_t thread;
    int counter = 0;

    // 创建线程
    err = infra_thread_create(&thread, thread_func, &counter);
    TEST_ASSERT(err == INFRA_OK);

    // 等待线程完成
    err = infra_thread_join(thread);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(counter == 1);
}

static void test_mutex(void) {
    infra_error_t err;
    infra_mutex_t mutex;
    int counter = 0;

    // 创建互斥锁
    err = infra_mutex_create(&mutex);
    TEST_ASSERT(err == INFRA_OK);

    // 加锁
    err = infra_mutex_lock(mutex);
    TEST_ASSERT(err == INFRA_OK);
    counter++;
    err = infra_mutex_unlock(mutex);
    TEST_ASSERT(err == INFRA_OK);

    // 尝试加锁
    err = infra_mutex_trylock(mutex);
    TEST_ASSERT(err == INFRA_OK);
    counter++;
    err = infra_mutex_unlock(mutex);
    TEST_ASSERT(err == INFRA_OK);

    // 销毁互斥锁
    infra_mutex_destroy(mutex);
    TEST_ASSERT(counter == 2);
}

static void test_cond(void) {
    infra_error_t err;
    infra_mutex_t mutex;
    infra_cond_t cond;

    // 创建互斥锁和条件变量
    err = infra_mutex_create(&mutex);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_cond_init(&cond);
    TEST_ASSERT(err == INFRA_OK);

    // 测试信号
    err = infra_mutex_lock(mutex);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_cond_signal(cond);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_mutex_unlock(mutex);
    TEST_ASSERT(err == INFRA_OK);

    // 测试广播
    err = infra_mutex_lock(mutex);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_cond_broadcast(cond);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_mutex_unlock(mutex);
    TEST_ASSERT(err == INFRA_OK);

    // 销毁条件变量和互斥锁
    infra_cond_destroy(cond);
    infra_mutex_destroy(mutex);
}

static void test_rwlock(void) {
    infra_error_t err;
    infra_rwlock_t rwlock;
    int counter = 0;

    // 创建读写锁
    err = infra_rwlock_init(&rwlock);
    TEST_ASSERT(err == INFRA_OK);

    // 读锁
    err = infra_rwlock_rdlock(rwlock);
    TEST_ASSERT(err == INFRA_OK);
    counter++;
    err = infra_rwlock_unlock(rwlock);
    TEST_ASSERT(err == INFRA_OK);

    // 写锁
    err = infra_rwlock_wrlock(rwlock);
    TEST_ASSERT(err == INFRA_OK);
    counter++;
    err = infra_rwlock_unlock(rwlock);
    TEST_ASSERT(err == INFRA_OK);

    // 销毁读写锁
    err = infra_rwlock_destroy(rwlock);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(counter == 2);
}

static void test_thread_pool(void) {
    infra_error_t err;
    infra_thread_pool_t* pool = NULL;
    
    // 创建线程池配置
    infra_thread_pool_config_t config = {
        .min_threads = 1,  // 减少线程数以降低复杂性
        .max_threads = 2,
        .queue_size = 5,   // 减少队列大小
        .idle_timeout = 100
    };
    
    // 创建线程池
    err = infra_thread_pool_create(&config, &pool);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(pool != NULL);
    
    // 测试任务计数器
    int counter = 0;
    
    // 提交任务
    err = infra_thread_pool_submit(pool, thread_func, &counter);
    TEST_ASSERT(err == INFRA_OK);
    
    // 等待任务完成
    infra_platform_sleep(200);  // 增加等待时间
    
    // 检查任务是否执行
    TEST_ASSERT(counter == 1);
    
    // 获取线程池统计信息
    size_t active_threads, queued_tasks;
    err = infra_thread_pool_get_stats(pool, &active_threads, &queued_tasks);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(queued_tasks == 0);  // 所有任务应该已完成
    
    // 销毁线程池
    err = infra_thread_pool_destroy(pool);
    TEST_ASSERT(err == INFRA_OK);
}

int main(void) {
    // 初始化infra系统
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }

    TEST_BEGIN();

    RUN_TEST(test_thread);
    RUN_TEST(test_mutex);
    RUN_TEST(test_cond);
    RUN_TEST(test_rwlock);
    // TODO: 等内存管理模块稳定后再启用线程池测试
    // RUN_TEST(test_thread_pool);

    TEST_END();

    // 清理infra系统
    infra_cleanup();
    return 0;
} 