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
    InfraxError err = thread->start(thread, test_thread_func, &test_value);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    
    // Check thread state after start
    if (!thread->is_running) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread->is_running", "Thread should be running after start");
    }
    
    // Get thread ID
    InfraxThreadId tid = thread->tid(thread);
    if (tid == 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "tid != 0", "Failed to get thread ID");
    }
    
    // Join thread
    void* result;
    err = thread->join(thread, &result);
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
        
        InfraxError err = threads[i]->start(threads[i], test_thread_func, &test_values[i]);
        if (err.code != 0) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
        }
    }
    
    // Join threads and verify results
    for (int i = 0; i < NUM_THREADS; i++) {
        void* result;
        InfraxError err = threads[i]->join(threads[i], &result);
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
        .name = NULL,
        .func = NULL,
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };
    
    InfraxThread* thread = InfraxThreadClass.new(&invalid_config);
    if (thread != NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread == NULL", "Thread creation with invalid config should fail");
    }
    
    // Test starting a thread with valid config but NULL function
    InfraxThreadConfig null_func_config = {
        .name = "test_thread",
        .func = NULL,
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };
    
    thread = InfraxThreadClass.new(&null_func_config);
    if (thread != NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread == NULL", "Thread creation with NULL function should fail");
    }
    
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
    
    // Start the thread before joining
    InfraxError err = thread->start(thread, test_thread_func, NULL);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", "Thread start should succeed");
    }

    // Clean up
    void* result;
    err = thread->join(thread, &result);
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
        .func = NULL,
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };
    
    InfraxThread* thread = InfraxThreadClass.new(&thread_config);
    if (!thread) {
        core->printf(core, "Failed to create thread pool manager\n");
        InfraxSyncClass.free(task_mutex);
        return;
    }
    
    // 配置线程池
    InfraxThreadPoolConfig pool_config = {
        .min_threads = 2,
        .max_threads = 4,
        .queue_size = 10
    };
    
    InfraxError err = infrax_thread_pool_create(thread, &pool_config);
    if (err.code != 0) {
        core->printf(core, "Failed to create thread pool: %s\n", err.message);
        InfraxThreadClass.free(thread);
        InfraxSyncClass.free(task_mutex);
        return;
    }
    
    // 提交任务
    int task_ids[5] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        err = infrax_thread_pool_submit(thread, pool_test_func, &task_ids[i]);
        if (err.code != 0) {
            core->printf(core, "Failed to submit task %d: %s\n", i + 1, err.message);
        }
    }
    
    // 等待所有任务完成
    core->sleep_ms(core, 1000);  // 简单等待
    
    // 检查任务计数器
    task_mutex->mutex_lock(task_mutex);
    if (task_counter != 5) {
        core->printf(core, "Task counter mismatch: expected 5, got %d\n", task_counter);
    }
    task_mutex->mutex_unlock(task_mutex);
    
    // 清理
    err = infrax_thread_pool_destroy(thread);
    if (err.code != 0) {
        core->printf(core, "Failed to destroy thread pool: %s\n", err.message);
    }
    
    InfraxThreadClass.free(thread);
    InfraxSyncClass.free(task_mutex);
    task_mutex = NULL;
    task_counter = 0;
    
    core->printf(core, "Thread pool basic test completed\n");
}

int main(void) {
    if (!core) core = InfraxCoreClass.singleton();
    core->printf(core, "===================\nStarting InfraxThread tests...\n");
    
    test_thread_basic();
    test_thread_multiple();
    test_thread_error_handling();
    test_thread_pool_basic();
    
    core->printf(core, "All infrax_thread tests passed!\n===================\n");
    return 0;
}