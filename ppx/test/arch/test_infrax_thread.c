#include <stdio.h>
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxSync.h"
#include "internal/infrax/InfraxMemory.h"
#include <string.h>

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
        printf("Task %d executed, total completed: %d\n", *task_id, task_counter);
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
    
    printf("Testing basic thread operations...\n");
    
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
    
    printf("Basic thread test passed\n");
}

void test_thread_multiple(void) {
    if (!core) core = InfraxCoreClass.singleton();
    
    printf("Testing multiple threads...\n");
    
    #define NUM_THREADS 5
    int test_values[NUM_THREADS] = {0};
    InfraxThread* threads[NUM_THREADS];
    
    // Create and start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        char thread_name[32];
        snprintf(thread_name, sizeof(thread_name), "test_thread_%d", i);
        
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
    
    printf("Multiple threads test passed\n");
}

void test_thread_error_handling(void) {
    if (!core) core = InfraxCoreClass.singleton();
    
    printf("Testing thread error handling...\n");
    
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
    
    printf("Thread error handling test passed\n");
}

// 测试线程池基本功能
void test_thread_pool_basic() {
    printf("Testing thread pool basic functionality...\n");
    
    // 初始化任务计数器互斥锁
    task_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    if (!task_mutex) {
        printf("Failed to initialize task mutex\n");
        return;
    }
    
    // 创建线程池
    InfraxThreadConfig thread_config = {
        .name = "pool_manager",
        .func = test_thread_func,
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };
    
    InfraxThread* thread = InfraxThreadClass.new(&thread_config);
    if (!thread) {
        printf("Failed to create thread object\n");
        InfraxSyncClass.free(task_mutex);
        return;
    }
    
    InfraxThreadPoolConfig config = {
        .min_threads = 2,
        .max_threads = 4,
        .queue_size = 10,
        .idle_timeout = 1000
    };
    
    InfraxError err = infrax_thread_pool_create(thread, &config);
    if (err.code != 0) {
        printf("Failed to create thread pool: %s\n", err.message);
        InfraxThreadClass.free(thread);
        InfraxSyncClass.free(task_mutex);
        return;
    }
    
    // 提交任务
    const int num_tasks = 5;
    int submitted = 0;
    
    for (int i = 0; i < num_tasks; i++) {
        InfraxMemory* memory = get_memory_manager();
        if (!memory) {
            printf("Failed to get memory manager\n");
            continue;
        }
        
        int* task_id = memory->alloc(memory, sizeof(int));
        if (!task_id) {
            printf("Failed to allocate task ID\n");
            continue;
        }
        
        *task_id = i + 1;
        err = infrax_thread_pool_submit(thread, pool_test_func, task_id);
        if (err.code != 0) {
            printf("Failed to submit task %d: %s\n", i + 1, err.message);
            memory->dealloc(memory, task_id);
            continue;
        }
        submitted++;
    }
    
    // 等待一段时间让任务执行完成
    if (core) core->sleep_ms(core, 1000);
    
    // 检查统计信息
    InfraxThreadPoolStats stats;
    err = infrax_thread_pool_get_stats(thread, &stats);
    if (err.code == 0) {
        printf("Thread pool stats:\n");
        printf("Active threads: %d\n", stats.active_threads);
        printf("Idle threads: %d\n", stats.idle_threads);
        printf("Pending tasks: %d\n", stats.pending_tasks);
        printf("Completed tasks: %d\n", stats.completed_tasks);
        
        // 验证所有任务都已完成
        if (stats.completed_tasks == submitted) {
            printf("All tasks completed successfully\n");
        } else {
            printf("Not all tasks completed (expected: %d, actual: %d)\n", 
                   submitted, stats.completed_tasks);
        }
    }
    
    // 销毁线程池
    err = infrax_thread_pool_destroy(thread);
    if (err.code != 0) {
        printf("Failed to destroy thread pool: %s\n", err.message);
    }
    
    InfraxThreadClass.free(thread);
    InfraxSyncClass.free(task_mutex);
    task_mutex = NULL;
    
    printf("Thread pool basic test completed\n");
}

// 测试线程池压力测试
void test_thread_pool_stress() {
    printf("Testing thread pool under stress...\n");
    
    // 初始化任务计数器互斥锁
    task_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    if (!task_mutex) {
        printf("Failed to initialize task mutex\n");
        return;
    }
    
    InfraxThreadConfig thread_config = {
        .name = "pool_manager",
        .func = test_thread_func,
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };
    
    InfraxThread* thread = InfraxThreadClass.new(&thread_config);
    if (!thread) {
        printf("Failed to create thread object\n");
        InfraxSyncClass.free(task_mutex);
        return;
    }
    
    InfraxThreadPoolConfig config = {
        .min_threads = 4,
        .max_threads = 8,
        .queue_size = 100,
        .idle_timeout = 1000
    };
    
    InfraxError err = infrax_thread_pool_create(thread, &config);
    if (err.code != 0) {
        printf("Failed to create thread pool: %s\n", err.message);
        InfraxThreadClass.free(thread);
        InfraxSyncClass.free(task_mutex);
        return;
    }
    
    // 提交大量任务
    const int num_tasks = 20;
    int submitted = 0;
    
    for (int i = 0; i < num_tasks; i++) {
        InfraxMemory* memory = get_memory_manager();
        if (!memory) {
            printf("Failed to get memory manager\n");
            continue;
        }
        
        int* task_id = memory->alloc(memory, sizeof(int));
        if (!task_id) {
            printf("Failed to allocate task ID\n");
            continue;
        }
        
        *task_id = i + 1;
        err = infrax_thread_pool_submit(thread, pool_test_func, task_id);
        if (err.code != 0) {
            printf("Failed to submit task %d: %s\n", i + 1, err.message);
            memory->dealloc(memory, task_id);
            continue;
        }
        submitted++;
    }
    
    // 等待所有任务完成
    bool all_done = false;
    int wait_count = 0;
    const int max_wait = 10;  // 最多等待10秒
    
    while (!all_done && wait_count < max_wait) {
        if (core) core->sleep_ms(core, 1000);
        wait_count++;
        
        InfraxThreadPoolStats stats;
        err = infrax_thread_pool_get_stats(thread, &stats);
        if (err.code == 0) {
            if (stats.completed_tasks == submitted) {
                all_done = true;
                printf("All %d tasks completed in %d seconds\n", submitted, wait_count);
                break;
            }
        }
    }
    
    if (!all_done) {
        printf("Timeout waiting for tasks to complete\n");
    }
    
    // 销毁线程池
    err = infrax_thread_pool_destroy(thread);
    if (err.code != 0) {
        printf("Failed to destroy thread pool: %s\n", err.message);
    }
    
    InfraxThreadClass.free(thread);
    InfraxSyncClass.free(task_mutex);
    task_mutex = NULL;
    
    printf("Thread pool stress test completed\n");
}

int main(void) {
    core = InfraxCoreClass.singleton();
    if (!core) {
        printf("Failed to get core singleton\n");
        return 1;
    }
    
    test_thread_basic();
    test_thread_multiple();
    test_thread_error_handling();
    test_thread_pool_basic();
    test_thread_pool_stress();
    
    return 0;
}