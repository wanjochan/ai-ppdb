#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "ppdb/internal.h"
#include "test/white/test_framework.h"
#include "test/white/test_macros.h"
#include "test/white/infra/test_sync.h"

// 测试配置
#define NUM_READERS 32
#define NUM_WRITERS 8
#define READ_ITERATIONS 10000
#define WRITE_ITERATIONS 1000
#define READ_WORK_ITERATIONS 100
#define WRITE_WORK_ITERATIONS 100
#define MAX_RETRIES 100  // 最大重试次数

// 调试模式控制
static bool is_debug_mode = false;

// 性能统计结构
typedef struct {
    atomic_uint_least64_t lock_attempts;
    atomic_uint_least64_t lock_successes;
    atomic_uint_least64_t total_wait_time_us;  // 使用微秒
    atomic_uint_least64_t total_work_time_us;  // 使用微秒
} thread_stats_t;

// 测试线程数据结构
typedef struct {
    ppdb_sync_t* sync;
    atomic_int* counter;
    int num_iterations;
    thread_stats_t stats;
    int thread_id;
} thread_data_t;

// 全局性能统计
typedef struct {
    atomic_uint_least64_t total_lock_attempts;
    atomic_uint_least64_t total_lock_successes;
    atomic_uint_least64_t total_wait_time_us;  // 使用微秒
    atomic_uint_least64_t total_work_time_us;  // 使用微秒
} global_stats_t;

static global_stats_t reader_stats = {0};
static global_stats_t writer_stats = {0};

// 添加时间测量函数
static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

static void print_elapsed(const char* stage, uint64_t start_us) {
    if (!is_debug_mode) return;
    uint64_t elapsed = get_time_us() - start_us;
    printf("\n[TIMING] %s took %.3f ms\n\n", stage, elapsed / 1000.0);
    fflush(stdout);
}

// 添加性能统计打印函数
static void print_stats(const char* type, const global_stats_t* stats) {
    uint64_t attempts = atomic_load(&stats->total_lock_attempts);
    uint64_t successes = atomic_load(&stats->total_lock_successes);
    uint64_t wait_time = atomic_load(&stats->total_wait_time_us);
    uint64_t work_time = atomic_load(&stats->total_work_time_us);
    
    printf("  Total attempts: %lu \n", attempts);
    printf("  Total successes: %lu\n", successes);
    printf("  Average wait time: %.2f ms\n", attempts > 0 ? (wait_time / 1000.0) / attempts : 0);
    printf("  Average work time: %.2f ms\n", successes > 0 ? (work_time / 1000.0) / successes : 0);
}

// 添加时间戳打印函数
static void print_timestamp(void) {
    if (!is_debug_mode) return;
    struct timeval tv;
    struct tm* tm_info;
    char buffer[32];
    
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    strftime(buffer, 26, "%y%m%d %H:%M:%S", tm_info);
    printf("%s.%03d ", buffer, (int)(tv.tv_usec / 1000));
}

// 修改调试打印宏
#define DEBUG_PRINT(...) do { \
    if (is_debug_mode) { \
        print_timestamp(); \
        printf(__VA_ARGS__); \
        fflush(stdout); \
    } \
} while(0)

#define INFO_PRINT(...) do { \
    printf(__VA_ARGS__); \
    fflush(stdout); \
} while(0)

// 互斥锁竞争测试的线程函数
static void* mutex_thread_func(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    for (int i = 0; i < data->num_iterations; i++) {
        while (true) {
            ppdb_error_t err = ppdb_sync_try_lock(data->sync);
            if (err == PPDB_OK) {
                atomic_fetch_add(data->counter, 1);
                ppdb_sync_unlock(data->sync);
                if (i % 100 == 0) {
                    printf("Thread completed %d iterations\n", i);
                }
                break;
            } else if (err == PPDB_ERR_BUSY) {
                usleep(1);
            } else {
                printf("Thread error: %d\n", err);
                return NULL;
            }
        }
    }
    printf("Thread completed all %d iterations\n", data->num_iterations);
    return NULL;
}

// 读写线程函数说明：
// 1. 测试例直接使用同步原语的返回值，不需要自己实现退让逻辑
// 2. 原语内部已经实现了退让机制（ppdb_sync_backoff）和重试次数控制
// 3. 原语的配置参数（backoff_us和max_retries）已经在创建时设置
// 4. 测试例只需关注功能正确性和性能统计

// 修改读线程函数
static void* reader_thread_func(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    for (int i = 0; i < data->num_iterations; i++) {
        uint64_t start_time = get_time_us();
        atomic_fetch_add(&data->stats.lock_attempts, 1);
        atomic_fetch_add(&reader_stats.total_lock_attempts, 1);
        
        ppdb_error_t err = ppdb_sync_read_lock(data->sync);
        if (err == PPDB_OK) {
            uint64_t lock_time = get_time_us();
            atomic_fetch_add(&data->stats.lock_successes, 1);
            atomic_fetch_add(&reader_stats.total_lock_successes, 1);
            
            uint64_t wait_time = lock_time - start_time;
            atomic_fetch_add(&data->stats.total_wait_time_us, wait_time);
            atomic_fetch_add(&reader_stats.total_wait_time_us, wait_time);
            
            // 模拟工作负载
            for (int j = 0; j < READ_WORK_ITERATIONS; j++) {
                atomic_load(data->counter);
            }
            
            ppdb_sync_read_unlock(data->sync);
            uint64_t end_time = get_time_us();
            uint64_t work_time = end_time - lock_time;
            atomic_fetch_add(&data->stats.total_work_time_us, work_time);
            atomic_fetch_add(&reader_stats.total_work_time_us, work_time);
            
            if (i % 1000 == 0 && is_debug_mode) {
                DEBUG_PRINT("[Reader %d] Progress: %d/%d\n", 
                    data->thread_id, i, data->num_iterations);
            }
        }
    }
    
    return NULL;
}

// 修改写线程函数
static void* writer_thread_func(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    for (int i = 0; i < data->num_iterations; i++) {
        uint64_t start_time = get_time_us();
        atomic_fetch_add(&data->stats.lock_attempts, 1);
        atomic_fetch_add(&writer_stats.total_lock_attempts, 1);
        
        ppdb_error_t err = ppdb_sync_write_lock(data->sync);
        if (err == PPDB_OK) {
            uint64_t lock_time = get_time_us();
            atomic_fetch_add(&data->stats.lock_successes, 1);
            atomic_fetch_add(&writer_stats.total_lock_successes, 1);
            
            uint64_t wait_time = lock_time - start_time;
            atomic_fetch_add(&data->stats.total_wait_time_us, wait_time);
            atomic_fetch_add(&writer_stats.total_wait_time_us, wait_time);
            
            // 模拟工作负载
            for (int j = 0; j < WRITE_WORK_ITERATIONS; j++) {
                atomic_fetch_add(data->counter, 1);
            }
            
            ppdb_sync_write_unlock(data->sync);
            uint64_t end_time = get_time_us();
            uint64_t work_time = end_time - lock_time;
            atomic_fetch_add(&data->stats.total_work_time_us, work_time);
            atomic_fetch_add(&writer_stats.total_work_time_us, work_time);
            
            if (i % 100 == 0 && is_debug_mode) {
                DEBUG_PRINT("[Writer %d] Progress: %d/%d\n", 
                    data->thread_id, i, data->num_iterations);
            }
        }
    }
    
    return NULL;
}

// 函数声明
void test_sync_basic(ppdb_sync_t* sync);
void test_rwlock(ppdb_sync_t* sync, bool use_lockfree);
void test_rwlock_concurrent(ppdb_sync_t* sync);

// 测试同步原语
void test_sync(bool use_lockfree) {
    printf("\n=== PPDB Synchronization Test Suite ===\n");
    printf("Test Mode: %s\n", use_lockfree ? "lockfree" : "locked");
    
    // 创建同步原语
    ppdb_sync_t* sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = use_lockfree,
        .max_readers = 32,
        .backoff_us = 1,
        .max_retries = 100
    };
    
    ppdb_error_t err = ppdb_sync_create(&sync, &config);
    assert(err == PPDB_OK);
    
    // 运行测试
    test_sync_basic(sync);
    test_rwlock(sync, use_lockfree);
    test_rwlock_concurrent(sync);
    
    // 清理
    ppdb_sync_destroy(sync);
    printf("\n=== All Tests Completed Successfully! ===\n");
}

// 测试基本锁操作
void test_sync_basic(ppdb_sync_t* sync) {
    // 基本锁操作测试
    ppdb_error_t err;
    
    // 测试基本锁定和解锁
    err = ppdb_sync_lock(sync);
    assert(err == PPDB_OK);
    err = ppdb_sync_unlock(sync);
    assert(err == PPDB_OK);
    
    // 测试尝试锁定
    err = ppdb_sync_try_lock(sync);
    assert(err == PPDB_OK);
    err = ppdb_sync_unlock(sync);
    assert(err == PPDB_OK);
}

// 测试读写锁基本操作
void test_rwlock(ppdb_sync_t* sync, bool use_lockfree) {
    // 读写锁基本测试
    ppdb_error_t err;
    
    // 测试读锁
    err = ppdb_sync_read_lock(sync);
    assert(err == PPDB_OK);
    err = ppdb_sync_read_unlock(sync);
    assert(err == PPDB_OK);
    
    // 测试写锁
    err = ppdb_sync_write_lock(sync);
    assert(err == PPDB_OK);
    err = ppdb_sync_write_unlock(sync);
    assert(err == PPDB_OK);
    
    // 测试尝试读锁
    err = ppdb_sync_try_read_lock(sync);
    assert(err == PPDB_OK);
    err = ppdb_sync_read_unlock(sync);
    assert(err == PPDB_OK);
    
    // 测试尝试写锁
    err = ppdb_sync_try_write_lock(sync);
    assert(err == PPDB_OK);
    err = ppdb_sync_write_unlock(sync);
    assert(err == PPDB_OK);
}

// 修改并发测试函数
void test_rwlock_concurrent(ppdb_sync_t* sync) {
    // 创建读写线程
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    thread_data_t reader_data[NUM_READERS];
    thread_data_t writer_data[NUM_WRITERS];
    atomic_int counter = 0;
    
    // 初始化全局统计
    memset(&reader_stats, 0, sizeof(global_stats_t));
    memset(&writer_stats, 0, sizeof(global_stats_t));
    
    // 创建读线程
    for (int i = 0; i < NUM_READERS; i++) {
        reader_data[i].sync = sync;
        reader_data[i].counter = &counter;
        reader_data[i].num_iterations = READ_ITERATIONS;
        reader_data[i].thread_id = i;
        memset(&reader_data[i].stats, 0, sizeof(thread_stats_t));
        int ret = pthread_create(&readers[i], NULL, reader_thread_func, &reader_data[i]);
        assert(ret == 0);
    }
    
    // 创建写线程
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_data[i].sync = sync;
        writer_data[i].counter = &counter;
        writer_data[i].num_iterations = WRITE_ITERATIONS;
        writer_data[i].thread_id = i;
        memset(&writer_data[i].stats, 0, sizeof(thread_stats_t));
        int ret = pthread_create(&writers[i], NULL, writer_thread_func, &writer_data[i]);
        assert(ret == 0);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }
    
    // 打印统计信息
    printf("\nPerformance Statistics:\n");
    printf("Readers:\n");
    print_stats("Readers", &reader_stats);
    
    printf("\nWriters:\n");
    print_stats("Writers", &writer_stats);
    
    printf("\nConcurrent rwlock test passed\n");
}

// 主测试函数
int main(void) {
    const char* build_mode = getenv("BUILD_MODE");
    is_debug_mode = (build_mode && strcmp(build_mode, "debug") == 0);

    INFO_PRINT("\n=== PPDB Synchronization Test Suite ===\n");
    
    const char* test_mode = getenv("PPDB_SYNC_MODE");
    if (!test_mode) {
        INFO_PRINT("Error: PPDB_SYNC_MODE environment variable not set\n");
        INFO_PRINT("Please set to either 'lockfree' or 'locked'\n");
        return 1;
    }

    INFO_PRINT("Test Mode: %s\n", test_mode);
    if (is_debug_mode) {
        INFO_PRINT("Build Mode: DEBUG\n");
    }
    INFO_PRINT("Starting tests...\n\n");

    if (strcmp(test_mode, "lockfree") == 0) {
        test_sync(true);
    } else if (strcmp(test_mode, "locked") == 0) {
        test_sync(false);
    } else {
        INFO_PRINT("Error: Invalid PPDB_SYNC_MODE: %s\n", test_mode);
        INFO_PRINT("Valid values are: 'lockfree' or 'locked'\n");
        return 1;
    }
    
    INFO_PRINT("\n=== All Tests Completed Successfully! ===\n");
    return 0;
}

