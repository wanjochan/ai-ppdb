#include <cosmopolitan.h>
#include "internal/base.h"
#include "../test_framework.h"

// 全局测试数据
static ppdb_base_t* g_base = NULL;

// 性能统计数据
typedef struct {
    int64_t total_ops;
    int64_t total_time_ns;
    int64_t min_time_ns;
    int64_t max_time_ns;
    double avg_time_ns;
    double ops_per_sec;
} perf_stats_t;

// 测试初始化
static int test_setup(void) {
    printf("\n=== Setting up sync performance test environment ===\n");
    
    // 初始化 base 配置
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024 * 100,  // 100MB
        .thread_pool_size = 8,
        .thread_safe = true,
        .enable_logging = true,
        .log_level = PPDB_LOG_DEBUG
    };
    
    // 初始化 base 层
    ASSERT_OK(ppdb_base_init(&g_base, &base_config));
    
    printf("Test environment setup completed\n");
    return 0;
}

// 测试清理
static int test_teardown(void) {
    printf("\n=== Cleaning up sync performance test environment ===\n");
    
    if (g_base) {
        ppdb_base_destroy(g_base);
        g_base = NULL;
    }
    
    printf("Test environment cleanup completed\n");
    return 0;
}

// 性能统计初始化
static void init_perf_stats(perf_stats_t* stats) {
    stats->total_ops = 0;
    stats->total_time_ns = 0;
    stats->min_time_ns = INT64_MAX;
    stats->max_time_ns = 0;
    stats->avg_time_ns = 0.0;
    stats->ops_per_sec = 0.0;
}

// 更新性能统计
static void update_perf_stats(perf_stats_t* stats, int64_t op_time_ns) {
    stats->total_ops++;
    stats->total_time_ns += op_time_ns;
    stats->min_time_ns = MIN(stats->min_time_ns, op_time_ns);
    stats->max_time_ns = MAX(stats->max_time_ns, op_time_ns);
    stats->avg_time_ns = (double)stats->total_time_ns / stats->total_ops;
    stats->ops_per_sec = 1e9 * stats->total_ops / stats->total_time_ns;
}

// 打印性能统计
static void print_perf_stats(const char* test_name, perf_stats_t* stats) {
    printf("\n=== Performance Statistics for %s ===\n", test_name);
    printf("Total Operations: %ld\n", stats->total_ops);
    printf("Total Time: %.2f ms\n", stats->total_time_ns / 1e6);
    printf("Min Time: %.2f us\n", stats->min_time_ns / 1e3);
    printf("Max Time: %.2f us\n", stats->max_time_ns / 1e3);
    printf("Avg Time: %.2f us\n", stats->avg_time_ns / 1e3);
    printf("Throughput: %.2f ops/sec\n", stats->ops_per_sec);
    printf("=====================================\n");
}

// 互斥锁性能测试
typedef struct {
    ppdb_base_mutex_t* mutex;
    int thread_id;
    int64_t iterations;
    perf_stats_t stats;
} mutex_thread_data_t;

static void* mutex_perf_thread(void* arg) {
    mutex_thread_data_t* data = (mutex_thread_data_t*)arg;
    int64_t start_time, end_time;
    
    for (int64_t i = 0; i < data->iterations; i++) {
        start_time = ppdb_base_get_time_ns();
        
        ASSERT_OK(ppdb_base_mutex_lock(data->mutex));
        // 模拟临界区操作
        ppdb_base_sleep_us(1);
        ASSERT_OK(ppdb_base_mutex_unlock(data->mutex));
        
        end_time = ppdb_base_get_time_ns();
        update_perf_stats(&data->stats, end_time - start_time);
    }
    
    return NULL;
}

static int test_mutex_performance(void) {
    printf("\n=== Running mutex performance test ===\n");
    
    const int num_threads = 4;
    const int64_t iterations_per_thread = 10000;
    
    // 创建互斥锁
    ppdb_base_mutex_t* mutex = NULL;
    ASSERT_OK(ppdb_base_mutex_create(&mutex));
    
    // 创建线程数据
    mutex_thread_data_t* thread_data = ppdb_base_malloc(sizeof(mutex_thread_data_t) * num_threads);
    ppdb_base_thread_t** threads = ppdb_base_malloc(sizeof(ppdb_base_thread_t*) * num_threads);
    
    // 启动线程
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].mutex = mutex;
        thread_data[i].thread_id = i;
        thread_data[i].iterations = iterations_per_thread;
        init_perf_stats(&thread_data[i].stats);
        
        ASSERT_OK(ppdb_base_thread_create(&threads[i], (ppdb_base_thread_func_t)mutex_perf_thread, &thread_data[i]));
    }
    
    // 等待线程完成
    for (int i = 0; i < num_threads; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i]));
    }
    
    // 合并统计数据
    perf_stats_t total_stats;
    init_perf_stats(&total_stats);
    
    for (int i = 0; i < num_threads; i++) {
        total_stats.total_ops += thread_data[i].stats.total_ops;
        total_stats.total_time_ns += thread_data[i].stats.total_time_ns;
        total_stats.min_time_ns = MIN(total_stats.min_time_ns, thread_data[i].stats.min_time_ns);
        total_stats.max_time_ns = MAX(total_stats.max_time_ns, thread_data[i].stats.max_time_ns);
    }
    
    total_stats.avg_time_ns = (double)total_stats.total_time_ns / total_stats.total_ops;
    total_stats.ops_per_sec = 1e9 * total_stats.total_ops / total_stats.total_time_ns;
    
    // 打印结果
    print_perf_stats("Mutex Performance Test", &total_stats);
    
    // 清理
    ppdb_base_mutex_destroy(mutex);
    ppdb_base_free(thread_data);
    ppdb_base_free(threads);
    
    return 0;
}

// 读写锁性能测试
typedef struct {
    ppdb_base_rwlock_t* rwlock;
    int thread_id;
    int64_t iterations;
    bool is_reader;
    perf_stats_t stats;
} rwlock_thread_data_t;

static void* rwlock_perf_thread(void* arg) {
    rwlock_thread_data_t* data = (rwlock_thread_data_t*)arg;
    int64_t start_time, end_time;
    
    for (int64_t i = 0; i < data->iterations; i++) {
        start_time = ppdb_base_get_time_ns();
        
        if (data->is_reader) {
            ASSERT_OK(ppdb_base_rwlock_rdlock(data->rwlock));
            // 模拟读操作
            ppdb_base_sleep_us(1);
            ASSERT_OK(ppdb_base_rwlock_unlock(data->rwlock));
        } else {
            ASSERT_OK(ppdb_base_rwlock_wrlock(data->rwlock));
            // 模拟写操作
            ppdb_base_sleep_us(2);
            ASSERT_OK(ppdb_base_rwlock_unlock(data->rwlock));
        }
        
        end_time = ppdb_base_get_time_ns();
        update_perf_stats(&data->stats, end_time - start_time);
    }
    
    return NULL;
}

static int test_rwlock_performance(void) {
    printf("\n=== Running rwlock performance test ===\n");
    
    const int num_readers = 3;
    const int num_writers = 1;
    const int num_threads = num_readers + num_writers;
    const int64_t iterations_per_thread = 10000;
    
    // 创建读写锁
    ppdb_base_rwlock_t* rwlock = NULL;
    ASSERT_OK(ppdb_base_rwlock_create(&rwlock));
    
    // 创建线程数据
    rwlock_thread_data_t* thread_data = ppdb_base_malloc(sizeof(rwlock_thread_data_t) * num_threads);
    ppdb_base_thread_t** threads = ppdb_base_malloc(sizeof(ppdb_base_thread_t*) * num_threads);
    
    // 启动线程
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].rwlock = rwlock;
        thread_data[i].thread_id = i;
        thread_data[i].iterations = iterations_per_thread;
        thread_data[i].is_reader = (i < num_readers);
        init_perf_stats(&thread_data[i].stats);
        
        ASSERT_OK(ppdb_base_thread_create(&threads[i], (ppdb_base_thread_func_t)rwlock_perf_thread, &thread_data[i]));
    }
    
    // 等待线程完成
    for (int i = 0; i < num_threads; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i]));
    }
    
    // 合并统计数据
    perf_stats_t reader_stats, writer_stats;
    init_perf_stats(&reader_stats);
    init_perf_stats(&writer_stats);
    
    for (int i = 0; i < num_threads; i++) {
        if (thread_data[i].is_reader) {
            reader_stats.total_ops += thread_data[i].stats.total_ops;
            reader_stats.total_time_ns += thread_data[i].stats.total_time_ns;
            reader_stats.min_time_ns = MIN(reader_stats.min_time_ns, thread_data[i].stats.min_time_ns);
            reader_stats.max_time_ns = MAX(reader_stats.max_time_ns, thread_data[i].stats.max_time_ns);
        } else {
            writer_stats.total_ops += thread_data[i].stats.total_ops;
            writer_stats.total_time_ns += thread_data[i].stats.total_time_ns;
            writer_stats.min_time_ns = MIN(writer_stats.min_time_ns, thread_data[i].stats.min_time_ns);
            writer_stats.max_time_ns = MAX(writer_stats.max_time_ns, thread_data[i].stats.max_time_ns);
        }
    }
    
    reader_stats.avg_time_ns = (double)reader_stats.total_time_ns / reader_stats.total_ops;
    reader_stats.ops_per_sec = 1e9 * reader_stats.total_ops / reader_stats.total_time_ns;
    
    writer_stats.avg_time_ns = (double)writer_stats.total_time_ns / writer_stats.total_ops;
    writer_stats.ops_per_sec = 1e9 * writer_stats.total_ops / writer_stats.total_time_ns;
    
    // 打印结果
    print_perf_stats("RWLock Reader Performance", &reader_stats);
    print_perf_stats("RWLock Writer Performance", &writer_stats);
    
    // 清理
    ppdb_base_rwlock_destroy(rwlock);
    ppdb_base_free(thread_data);
    ppdb_base_free(threads);
    
    return 0;
}

int main(void) {
    printf("Running sync performance tests...\n");
    
    TEST_INIT();
    
    if (test_setup() != 0) {
        printf("Test setup failed\n");
        return 1;
    }
    
    test_case_t test_cases[] = {
        {
            .name = "mutex_performance",
            .description = "Test mutex performance",
            .fn = test_mutex_performance,
            .timeout_seconds = 60,
            .skip = false
        },
        {
            .name = "rwlock_performance",
            .description = "Test rwlock performance",
            .fn = test_rwlock_performance,
            .timeout_seconds = 60,
            .skip = false
        }
    };
    
    test_suite_t suite = {
        .name = "Sync Performance Tests",
        .setup = NULL,
        .teardown = NULL,
        .cases = test_cases,
        .num_cases = sizeof(test_cases) / sizeof(test_cases[0])
    };
    
    int result = run_test_suite(&suite);
    
    if (test_teardown() != 0) {
        printf("Test teardown failed\n");
        return 1;
    }
    
    TEST_CLEANUP();
    test_print_stats();
    
    return result;
} 