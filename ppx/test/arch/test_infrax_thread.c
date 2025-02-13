#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxSync.h"
#include "internal/infrax/InfraxMemory.h"

// Function declarations
InfraxMemory* get_memory_manager(void);

// Thread pool function declarations
InfraxError infrax_thread_pool_create(InfraxThread* self, InfraxThreadPoolConfig* config);
InfraxError infrax_thread_pool_destroy(InfraxThread* self);
InfraxError infrax_thread_pool_submit(InfraxThread* self, InfraxThreadFunc func, void* arg);
InfraxError infrax_thread_pool_get_stats(InfraxThread* self, InfraxThreadPoolStats* stats);

// Get singleton instance of InfraxCore
static InfraxCore* core = NULL;

// 线程池测试用的互斥锁和计数器
static InfraxSync* task_mutex = NULL;
static int task_counter = 0;

// 新增测试函数声明
void test_thread_stress(void);
void test_thread_sync_complex(void);

// 共享资源和同步原语
static InfraxSync* stress_mutex = NULL;
static int64_t stress_counter = 0;
static InfraxSync* producer_consumer_mutex = NULL;
static InfraxSync* producer_consumer_cond = NULL;
static int producer_consumer_queue[10];
static int queue_head = 0;
static int queue_tail = 0;
static bool queue_full = false;

// 线程池测试用的任务函数
static void* pool_test_func(void* arg) {
    int* task_id = (int*)arg;
    if (!task_id) return NULL;
    
    // 模拟任务执行
    core->sleep_ms(core, 100);  // 短暂延迟
    
    // 更新计数器
    if (task_mutex) {
        task_mutex->mutex_lock(task_mutex);
        task_counter++;
        core->printf(core, "Task %d executed, total completed: %d\n", *task_id, task_counter);
        task_mutex->mutex_unlock(task_mutex);
    }
    
    return NULL;
}

static void* test_thread_func(void* arg) {
    int* value = (int*)arg;
    if (value) {
        (*value)++;
    }
    return value;
}

void test_thread_basic(void) {
    if (!core) core = InfraxCoreClass.singleton();
    
    core->printf(core, "Testing basic thread operations...\n");
    
    int test_value = 0;
    InfraxThreadConfig config = {
        .name = "test_thread",
        .func = test_thread_func,
        .arg = &test_value,
        .stack_size = 0,  // 使用默认值
        .priority = 0     // 使用默认值
    };
    
    // Create thread instance
    InfraxThread* thread = InfraxThreadClass.new(&config);
    if (thread == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread != NULL", "Failed to create thread");
    }
    
    // Check initial thread state
    if (thread->is_running) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "!thread->is_running", "Thread should not be running initially");
    }
    
    // Start thread
    InfraxError err = InfraxThreadClass.start(thread, test_thread_func, &test_value);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    
    // Check thread state after start
    if (!thread->is_running) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread->is_running", "Thread should be running after start");
    }
    
    // Get thread ID
    InfraxThreadId tid = InfraxThreadClass.tid(thread);
    if (tid == 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "tid != 0", "Failed to get thread ID");
    }
    
    // Join thread
    void* result;
    err = InfraxThreadClass.join(thread, &result);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    
    // Check thread state after join
    if (thread->is_running) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "!thread->is_running", "Thread should not be running after join");
    }
    
    // Check result
    if (test_value != 1) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "test_value == 1", "Thread function did not execute properly");
    }
    if (*(int*)result != 1) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "*(int*)result == 1", "Thread return value is incorrect");
    }
    
    // Clean up
    InfraxThreadClass.free(thread);
    
    core->printf(core, "Basic thread test passed\n");
}

void test_thread_multiple(void) {
    if (!core) core = InfraxCoreClass.singleton();
    
    core->printf(core, "Testing multiple threads...\n");
    
    #define NUM_THREADS 5
    int test_values[NUM_THREADS] = {0};
    InfraxThread* threads[NUM_THREADS];
    
    // Create and start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        char thread_name[32];
        core->snprintf(core, thread_name, sizeof(thread_name), "test_thread_%d", i);
        
        InfraxThreadConfig config = {
            .name = thread_name,
            .func = test_thread_func,
            .arg = &test_values[i],
            .stack_size = 0,
            .priority = 0
        };
        
        threads[i] = InfraxThreadClass.new(&config);
        if (threads[i] == NULL) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "threads[i] != NULL", "Failed to create thread");
        }
        
        InfraxError err = InfraxThreadClass.start(threads[i], test_thread_func, &test_values[i]);
        if (err.code != 0) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
        }
    }
    
    // Join threads and verify results
    for (int i = 0; i < NUM_THREADS; i++) {
        void* result;
        InfraxError err = InfraxThreadClass.join(threads[i], &result);
        if (err.code != 0) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
        }
        if (test_values[i] != 1) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "test_values[i] == 1", "Thread function did not execute properly");
        }
        if (*(int*)result != 1) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "*(int*)result == 1", "Thread return value is incorrect");
        }
        
        InfraxThreadClass.free(threads[i]);
    }
    
    core->printf(core, "Multiple threads test passed\n");
}

void test_thread_error_handling(void) {
    if (!core) core = InfraxCoreClass.singleton();
    
    core->printf(core, "Testing thread error handling...\n");
    
    // Test invalid config
    InfraxThreadConfig invalid_config = {
        .name = NULL,  // 只有name为NULL时才应该失败
        .func = NULL,
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };
    
    InfraxThread* thread = InfraxThreadClass.new(&invalid_config);
    if (thread != NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread == NULL", "Thread creation with NULL name should fail");
    }
    
    // Test starting a thread with NULL function
    InfraxThreadConfig null_func_config = {
        .name = "test_thread",
        .func = NULL,
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };
    
    thread = InfraxThreadClass.new(&null_func_config);
    if (thread == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread != NULL", "Thread creation with NULL function should succeed");
    }
    
    // 尝试启动没有函数的线程应该失败
    InfraxError err = InfraxThreadClass.start(thread, NULL, NULL);
    if (err.code == 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code != 0", "Starting thread with NULL function should fail");
    }
    
    // Clean up
    InfraxThreadClass.free(thread);
    
    // Test starting a valid thread twice
    InfraxThreadConfig valid_config = {
        .name = "test_thread",
        .func = test_thread_func,
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };
    
    thread = InfraxThreadClass.new(&valid_config);
    if (thread == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread != NULL", "Thread creation with valid config should succeed");
    }
    
    // Start the thread
    err = InfraxThreadClass.start(thread, test_thread_func, NULL);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", "Thread start should succeed");
    }
    
    // Try to start it again (should fail)
    err = InfraxThreadClass.start(thread, test_thread_func, NULL);
    if (err.code == 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code != 0", "Starting thread twice should fail");
    }
    
    // Clean up
    void* result;
    err = InfraxThreadClass.join(thread, &result);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", "Join should succeed");
    }
    
    InfraxThreadClass.free(thread);
    
    core->printf(core, "Thread error handling test passed\n");
}

// 测试线程池基本功能
void test_thread_pool_basic() {
    if (!core) core = InfraxCoreClass.singleton();
    
    core->printf(core, "Testing thread pool basic functionality...\n");
    
    // 初始化任务计数器互斥锁
    task_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    if (!task_mutex) {
        core->printf(core, "Failed to initialize task mutex\n");
        return;
    }
    
    // 创建线程池
    InfraxThreadConfig thread_config = {
        .name = "pool_manager",
        .func = NULL,  // 线程池管理器不需要初始函数
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };
    
    core->printf(core, "Creating thread pool manager...\n");
    InfraxThread* thread = InfraxThreadClass.new(&thread_config);
    if (!thread) {
        core->printf(core, "Failed to create thread pool manager\n");
        InfraxSyncClass.free(task_mutex);
        return;
    }
    core->printf(core, "Thread pool manager created successfully\n");
    
    // 配置线程池
    InfraxThreadPoolConfig pool_config = {
        .min_threads = 2,
        .max_threads = 4,
        .queue_size = 10,
        .idle_timeout = 1000
    };
    
    core->printf(core, "Initializing thread pool...\n");
    InfraxError err = InfraxThreadClass.pool_create(thread, &pool_config);
    if (err.code != 0) {
        core->printf(core, "Failed to create thread pool: %s (code: %d)\n", err.message, err.code);
        InfraxThreadClass.free(thread);
        InfraxSyncClass.free(task_mutex);
        return;
    }
    core->printf(core, "Thread pool initialized successfully\n");
    
    // 提交任务
    core->printf(core, "Submitting tasks to thread pool...\n");
    int task_ids[5] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        err = InfraxThreadClass.pool_submit(thread, pool_test_func, &task_ids[i]);
        if (err.code != 0) {
            core->printf(core, "Failed to submit task %d: %s (code: %d)\n", i + 1, err.message, err.code);
        } else {
            core->printf(core, "Successfully submitted task %d\n", i + 1);
        }
    }
    
    // 等待所有任务完成
    core->printf(core, "Waiting for tasks to complete...\n");
    core->sleep_ms(core, 1000);  // 简单等待
    
    // 获取线程池统计信息
    InfraxThreadPoolStats stats;
    err = InfraxThreadClass.pool_get_stats(thread, &stats);
    if (err.code == 0) {
        core->printf(core, "Thread pool stats:\n");
        core->printf(core, "  Active threads: %d\n", stats.active_threads);
        core->printf(core, "  Idle threads: %d\n", stats.idle_threads);
        core->printf(core, "  Pending tasks: %d\n", stats.pending_tasks);
        core->printf(core, "  Completed tasks: %d\n", stats.completed_tasks);
    } else {
        core->printf(core, "Failed to get thread pool stats: %s (code: %d)\n", err.message, err.code);
    }
    
    // 检查任务计数器
    task_mutex->mutex_lock(task_mutex);
    if (task_counter != 5) {
        core->printf(core, "Task counter mismatch: expected 5, got %d\n", task_counter);
    } else {
        core->printf(core, "All tasks completed successfully\n");
    }
    task_mutex->mutex_unlock(task_mutex);
    
    // 清理
    core->printf(core, "Cleaning up thread pool...\n");
    err = InfraxThreadClass.pool_destroy(thread);
    if (err.code != 0) {
        core->printf(core, "Failed to destroy thread pool: %s (code: %d)\n", err.message, err.code);
    } else {
        core->printf(core, "Thread pool destroyed successfully\n");
    }
    
    InfraxThreadClass.free(thread);
    InfraxSyncClass.free(task_mutex);
    task_mutex = NULL;
    task_counter = 0;
    
    core->printf(core, "Thread pool basic test completed\n");
}

// 压力测试线程函数
static void* stress_thread_func(void* arg) {
    int* iterations = (int*)arg;
    for(int i = 0; i < *iterations; i++) {
        stress_mutex->mutex_lock(stress_mutex);
        stress_counter++;
        stress_mutex->mutex_unlock(stress_mutex);
        core->hint_yield(core);  // 提示让出CPU
    }
    return NULL;
}

// 生产者线程函数
static void* producer_func(void* arg) {
    int* items = (int*)arg;
    int produced = 0;
    
    while(produced < *items) {
        producer_consumer_mutex->mutex_lock(producer_consumer_mutex);
        
        while(queue_full) {
            producer_consumer_cond->cond_wait(producer_consumer_cond, producer_consumer_mutex);
        }
        
        producer_consumer_queue[queue_tail] = produced;
        queue_tail = (queue_tail + 1) % 10;
        queue_full = (queue_head == queue_tail);
        produced++;
        
        producer_consumer_cond->cond_signal(producer_consumer_cond);
        producer_consumer_mutex->mutex_unlock(producer_consumer_mutex);
        
        core->sleep_ms(core, 1);  // 模拟生产耗时
    }
    
    return NULL;
}

// 消费者线程函数
static void* consumer_func(void* arg) {
    int* items = (int*)arg;
    int consumed = 0;
    
    while(consumed < *items) {
        producer_consumer_mutex->mutex_lock(producer_consumer_mutex);
        
        while(queue_head == queue_tail && !queue_full) {
            producer_consumer_cond->cond_wait(producer_consumer_cond, producer_consumer_mutex);
        }
        
        int value = producer_consumer_queue[queue_head];
        queue_head = (queue_head + 1) % 10;
        queue_full = false;
        consumed++;
        
        producer_consumer_cond->cond_signal(producer_consumer_cond);
        producer_consumer_mutex->mutex_unlock(producer_consumer_mutex);
        
        core->sleep_ms(core, 2);  // 模拟消费耗时
    }
    
    return NULL;
}

void test_thread_stress(void) {
    if (!core) core = InfraxCoreClass.singleton();
    
    core->printf(core, "Testing thread stress...\n");
    
    #define STRESS_THREAD_COUNT 50
    #define ITERATIONS_PER_THREAD 1000
    
    // 初始化同步原语
    stress_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    if (!stress_mutex) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "stress_mutex != NULL", "Failed to create stress mutex");
        return;
    }
    
    // 创建并启动大量线程
    InfraxThread* threads[STRESS_THREAD_COUNT];
    int iterations[STRESS_THREAD_COUNT];
    
    for(int i = 0; i < STRESS_THREAD_COUNT; i++) {
        iterations[i] = ITERATIONS_PER_THREAD;
        
        char thread_name[32];
        core->snprintf(core, thread_name, sizeof(thread_name), "stress_thread_%d", i);
        
        InfraxThreadConfig config = {
            .name = thread_name,
            .func = stress_thread_func,
            .arg = &iterations[i],
            .stack_size = 0,
            .priority = 0
        };
        
        threads[i] = InfraxThreadClass.new(&config);
        if (!threads[i]) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "threads[i] != NULL", "Failed to create thread");
            continue;
        }
        
        InfraxError err = InfraxThreadClass.start(threads[i], stress_thread_func, &iterations[i]);
        if (err.code != 0) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
        }
    }
    
    // 等待所有线程完成
    for(int i = 0; i < STRESS_THREAD_COUNT; i++) {
        if (threads[i]) {
            void* result;
            InfraxError err = InfraxThreadClass.join(threads[i], &result);
            if (err.code != 0) {
                core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
            }
            InfraxThreadClass.free(threads[i]);
        }
    }
    
    // 验证结果
    if (stress_counter != (int64_t)STRESS_THREAD_COUNT * ITERATIONS_PER_THREAD) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, 
            "stress_counter == STRESS_THREAD_COUNT * ITERATIONS_PER_THREAD",
            "Counter value incorrect after stress test");
    }
    
    InfraxSyncClass.free(stress_mutex);
    stress_mutex = NULL;
    stress_counter = 0;
    
    core->printf(core, "Thread stress test passed\n");
}

void test_thread_sync_complex(void) {
    if (!core) core = InfraxCoreClass.singleton();
    
    core->printf(core, "Testing complex thread synchronization...\n");
    
    // 初始化同步原语
    producer_consumer_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    producer_consumer_cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
    
    if (!producer_consumer_mutex || !producer_consumer_cond) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, 
            "producer_consumer_mutex != NULL && producer_consumer_cond != NULL",
            "Failed to create synchronization primitives");
        return;
    }
    
    // 创建生产者和消费者线程
    int items_to_produce = 100;
    
    InfraxThreadConfig producer_config = {
        .name = "producer",
        .func = producer_func,
        .arg = &items_to_produce,
        .stack_size = 0,
        .priority = 0
    };
    
    InfraxThreadConfig consumer_config = {
        .name = "consumer",
        .func = consumer_func,
        .arg = &items_to_produce,
        .stack_size = 0,
        .priority = 0
    };
    
    InfraxThread* producer = InfraxThreadClass.new(&producer_config);
    InfraxThread* consumer = InfraxThreadClass.new(&consumer_config);
    
    if (!producer || !consumer) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, 
            "producer != NULL && consumer != NULL",
            "Failed to create producer/consumer threads");
        return;
    }
    
    // 启动线程
    InfraxError err = InfraxThreadClass.start(producer, producer_func, &items_to_produce);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    
    err = InfraxThreadClass.start(consumer, consumer_func, &items_to_produce);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    
    // 等待线程完成
    void* result;
    err = InfraxThreadClass.join(producer, &result);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    
    err = InfraxThreadClass.join(consumer, &result);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    
    // 清理
    InfraxThreadClass.free(producer);
    InfraxThreadClass.free(consumer);
    InfraxSyncClass.free(producer_consumer_mutex);
    InfraxSyncClass.free(producer_consumer_cond);
    producer_consumer_mutex = NULL;
    producer_consumer_cond = NULL;
    queue_head = queue_tail = 0;
    queue_full = false;
    
    core->printf(core, "Complex thread synchronization test passed\n");
}

int main(void) {
    if (!core) core = InfraxCoreClass.singleton();
    core->printf(core, "===================\nStarting InfraxThread tests...\n");
    
    test_thread_basic();
    test_thread_multiple();
    test_thread_error_handling();
    test_thread_pool_basic();
    test_thread_stress();
    test_thread_sync_complex();
    
    core->printf(core, "All infrax_thread tests passed!\n===================\n");
    return 0;
}