#include "../../white/test_framework.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_internal.h"
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

// 性能测试配置
#define WARM_UP_COUNT 1000            // 预热操作数
#define TEST_COUNT 100000             // 测试操作数
#define KEY_SIZE 16                   // 键大小
#define SMALL_VALUE_SIZE 64          // 小值大小
#define LARGE_VALUE_SIZE (16 * 1024) // 大值大小 (16KB)
#define BATCH_SIZE 1000              // 批处理大小
#define NUM_THREADS 4                // 线程数

// 性能统计
typedef struct {
    double min_latency;    // 最小延迟（微秒）
    double max_latency;    // 最大延迟（微秒）
    double avg_latency;    // 平均延迟（微秒）
    double p95_latency;    // 95分位延迟（微秒）
    double p99_latency;    // 99分位延迟（微秒）
    double throughput;     // 吞吐量（ops/s）
    size_t total_bytes;    // 总处理字节数
} perf_stats_t;

// 线程上下文
typedef struct {
    ppdb_kvstore_t* store;
    int thread_id;
    perf_stats_t stats;
    double* latencies;    // 记录每个操作的延迟
    size_t op_count;
} thread_context_t;

// 获取当前时间（微秒）
static uint64_t get_microseconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

// 计算统计信息
static void calculate_stats(double* latencies, size_t count, perf_stats_t* stats) {
    if (count == 0) return;

    // 排序延迟数组用于计算分位数
    qsort(latencies, count, sizeof(double), double_compare);

    stats->min_latency = latencies[0];
    stats->max_latency = latencies[count - 1];

    // 计算平均延迟
    double sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += latencies[i];
    }
    stats->avg_latency = sum / count;

    // 计算分位数
    stats->p95_latency = latencies[(size_t)(count * 0.95)];
    stats->p99_latency = latencies[(size_t)(count * 0.99)];

    // 计算吞吐量（ops/s）
    double total_time = sum / 1000000.0; // 转换为秒
    stats->throughput = count / total_time;
}

// 比较函数用于qsort
static int double_compare(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

// Memtable 操作性能测试
static int benchmark_memtable_ops(void) {
    ppdb_kvstore_t* store = NULL;
    int err;
    perf_stats_t write_stats = {0}, read_stats = {0};
    double* write_latencies = malloc(TEST_COUNT * sizeof(double));
    double* read_latencies = malloc(TEST_COUNT * sizeof(double));
    TEST_ASSERT_NOT_NULL(write_latencies, "Failed to allocate write latencies array");
    TEST_ASSERT_NOT_NULL(read_latencies, "Failed to allocate read latencies array");

    // 创建KVStore（禁用WAL以测试纯内存表性能）
    ppdb_config_t config = {
        .enable_wal = false,
        .memtable_size = 64 * 1024 * 1024  // 64MB
    };
    
    err = ppdb_kvstore_create(&store, &config);
    TEST_ASSERT_OK(err, "Failed to create kvstore");

    // 准备测试数据
    char key[KEY_SIZE];
    char value[SMALL_VALUE_SIZE];
    memset(value, 'v', SMALL_VALUE_SIZE - 1);
    value[SMALL_VALUE_SIZE - 1] = '\0';

    // 预热
    printf("Warming up...\n");
    for (int i = 0; i < WARM_UP_COUNT; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
        TEST_ASSERT_OK(err, "Warm-up put failed");
    }

    // 写入测试
    printf("Testing writes...\n");
    for (int i = 0; i < TEST_COUNT; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        
        uint64_t start = get_microseconds();
        err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
        uint64_t end = get_microseconds();
        
        TEST_ASSERT_OK(err, "Write test failed");
        write_latencies[i] = end - start;
        write_stats.total_bytes += strlen(key) + strlen(value);
    }

    // 读取测试
    printf("Testing reads...\n");
    char read_value[SMALL_VALUE_SIZE];
    for (int i = 0; i < TEST_COUNT; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        
        uint64_t start = get_microseconds();
        err = ppdb_kvstore_get(store, key, strlen(key), read_value, sizeof(read_value));
        uint64_t end = get_microseconds();
        
        TEST_ASSERT_OK(err, "Read test failed");
        read_latencies[i] = end - start;
        read_stats.total_bytes += strlen(read_value);
    }

    // 计算统计信息
    calculate_stats(write_latencies, TEST_COUNT, &write_stats);
    calculate_stats(read_latencies, TEST_COUNT, &read_stats);

    // 输出结果
    printf("\nWrite Performance:\n");
    printf("Throughput: %.2f ops/s\n", write_stats.throughput);
    printf("Average Latency: %.2f us\n", write_stats.avg_latency);
    printf("P95 Latency: %.2f us\n", write_stats.p95_latency);
    printf("P99 Latency: %.2f us\n", write_stats.p99_latency);
    printf("Total Data Written: %.2f MB\n", write_stats.total_bytes / (1024.0 * 1024.0));

    printf("\nRead Performance:\n");
    printf("Throughput: %.2f ops/s\n", read_stats.throughput);
    printf("Average Latency: %.2f us\n", read_stats.avg_latency);
    printf("P95 Latency: %.2f us\n", read_stats.p95_latency);
    printf("P99 Latency: %.2f us\n", read_stats.p99_latency);
    printf("Total Data Read: %.2f MB\n", read_stats.total_bytes / (1024.0 * 1024.0));

    free(write_latencies);
    free(read_latencies);
    ppdb_destroy(store);
    return 0;
}

// WAL 操作性能测试
static int benchmark_wal_ops(void) {
    ppdb_kvstore_t* store = NULL;
    int err;
    perf_stats_t sync_stats = {0}, async_stats = {0};
    double* sync_latencies = malloc(TEST_COUNT * sizeof(double));
    double* async_latencies = malloc(TEST_COUNT * sizeof(double));
    TEST_ASSERT_NOT_NULL(sync_latencies, "Failed to allocate sync latencies array");
    TEST_ASSERT_NOT_NULL(async_latencies, "Failed to allocate async latencies array");

    // 测试同步写入
    ppdb_config_t sync_config = {
        .enable_wal = true,
        .wal_path = "/tmp/ppdb_wal_sync",
        .sync_write = true
    };
    
    err = ppdb_kvstore_create(&store, &sync_config);
    TEST_ASSERT_OK(err, "Failed to create sync store");

    // 准备测试数据
    char key[KEY_SIZE];
    char value[LARGE_VALUE_SIZE];
    memset(value, 'v', LARGE_VALUE_SIZE - 1);
    value[LARGE_VALUE_SIZE - 1] = '\0';

    // 同步写入测试
    printf("Testing synchronous WAL writes...\n");
    for (int i = 0; i < TEST_COUNT; i++) {
        snprintf(key, sizeof(key), "sync_key_%d", i);
        
        uint64_t start = get_microseconds();
        err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
        uint64_t end = get_microseconds();
        
        TEST_ASSERT_OK(err, "Sync write failed");
        sync_latencies[i] = end - start;
        sync_stats.total_bytes += strlen(key) + strlen(value);
    }

    ppdb_destroy(store);

    // 测试异步写入
    ppdb_config_t async_config = {
        .enable_wal = true,
        .wal_path = "/tmp/ppdb_wal_async",
        .sync_write = false
    };
    
    err = ppdb_kvstore_create(&store, &async_config);
    TEST_ASSERT_OK(err, "Failed to create async store");

    // 异步写入测试
    printf("Testing asynchronous WAL writes...\n");
    for (int i = 0; i < TEST_COUNT; i++) {
        snprintf(key, sizeof(key), "async_key_%d", i);
        
        uint64_t start = get_microseconds();
        err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
        uint64_t end = get_microseconds();
        
        TEST_ASSERT_OK(err, "Async write failed");
        async_latencies[i] = end - start;
        async_stats.total_bytes += strlen(key) + strlen(value);
    }

    // 计算统计信息
    calculate_stats(sync_latencies, TEST_COUNT, &sync_stats);
    calculate_stats(async_latencies, TEST_COUNT, &async_stats);

    // 输出结果
    printf("\nSynchronous WAL Performance:\n");
    printf("Throughput: %.2f ops/s\n", sync_stats.throughput);
    printf("Average Latency: %.2f us\n", sync_stats.avg_latency);
    printf("P95 Latency: %.2f us\n", sync_stats.p95_latency);
    printf("P99 Latency: %.2f us\n", sync_stats.p99_latency);
    printf("Total Data Written: %.2f MB\n", sync_stats.total_bytes / (1024.0 * 1024.0));

    printf("\nAsynchronous WAL Performance:\n");
    printf("Throughput: %.2f ops/s\n", async_stats.throughput);
    printf("Average Latency: %.2f us\n", async_stats.avg_latency);
    printf("P95 Latency: %.2f us\n", async_stats.p95_latency);
    printf("P99 Latency: %.2f us\n", async_stats.p99_latency);
    printf("Total Data Written: %.2f MB\n", async_stats.total_bytes / (1024.0 * 1024.0));

    free(sync_latencies);
    free(async_latencies);
    ppdb_destroy(store);
    cleanup_test_dir("/tmp/ppdb_wal_sync");
    cleanup_test_dir("/tmp/ppdb_wal_async");
    return 0;
}

// 压缩性能测试
static int benchmark_compression(void) {
    ppdb_kvstore_t* store = NULL;
    int err;
    perf_stats_t comp_stats = {0}, no_comp_stats = {0};
    double* comp_latencies = malloc(TEST_COUNT * sizeof(double));
    double* no_comp_latencies = malloc(TEST_COUNT * sizeof(double));
    TEST_ASSERT_NOT_NULL(comp_latencies, "Failed to allocate compression latencies array");
    TEST_ASSERT_NOT_NULL(no_comp_latencies, "Failed to allocate no-compression latencies array");

    // 准备可压缩的测试数据（重复模式）
    char value[LARGE_VALUE_SIZE];
    for (int i = 0; i < LARGE_VALUE_SIZE - 1; i++) {
        value[i] = 'a' + (i % 26);
    }
    value[LARGE_VALUE_SIZE - 1] = '\0';

    // 测试启用压缩
    ppdb_config_t comp_config = {
        .compression_enabled = true,
        .memtable_size = 64 * 1024 * 1024
    };
    
    err = ppdb_kvstore_create(&store, &comp_config);
    TEST_ASSERT_OK(err, "Failed to create compressed store");

    printf("Testing with compression...\n");
    char key[KEY_SIZE];
    for (int i = 0; i < TEST_COUNT; i++) {
        snprintf(key, sizeof(key), "comp_key_%d", i);
        
        uint64_t start = get_microseconds();
        err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
        uint64_t end = get_microseconds();
        
        TEST_ASSERT_OK(err, "Compressed write failed");
        comp_latencies[i] = end - start;
        comp_stats.total_bytes += strlen(key) + strlen(value);
    }

    ppdb_destroy(store);

    // 测试禁用压缩
    ppdb_config_t no_comp_config = {
        .compression_enabled = false,
        .memtable_size = 64 * 1024 * 1024
    };
    
    err = ppdb_kvstore_create(&store, &no_comp_config);
    TEST_ASSERT_OK(err, "Failed to create uncompressed store");

    printf("Testing without compression...\n");
    for (int i = 0; i < TEST_COUNT; i++) {
        snprintf(key, sizeof(key), "no_comp_key_%d", i);
        
        uint64_t start = get_microseconds();
        err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
        uint64_t end = get_microseconds();
        
        TEST_ASSERT_OK(err, "Uncompressed write failed");
        no_comp_latencies[i] = end - start;
        no_comp_stats.total_bytes += strlen(key) + strlen(value);
    }

    // 计算统计信息
    calculate_stats(comp_latencies, TEST_COUNT, &comp_stats);
    calculate_stats(no_comp_latencies, TEST_COUNT, &no_comp_stats);

    // 输出结果
    printf("\nCompressed Write Performance:\n");
    printf("Throughput: %.2f ops/s\n", comp_stats.throughput);
    printf("Average Latency: %.2f us\n", comp_stats.avg_latency);
    printf("P95 Latency: %.2f us\n", comp_stats.p95_latency);
    printf("P99 Latency: %.2f us\n", comp_stats.p99_latency);
    printf("Total Data Written: %.2f MB\n", comp_stats.total_bytes / (1024.0 * 1024.0));

    printf("\nUncompressed Write Performance:\n");
    printf("Throughput: %.2f ops/s\n", no_comp_stats.throughput);
    printf("Average Latency: %.2f us\n", no_comp_stats.avg_latency);
    printf("P95 Latency: %.2f us\n", no_comp_stats.p95_latency);
    printf("P99 Latency: %.2f us\n", no_comp_stats.p99_latency);
    printf("Total Data Written: %.2f MB\n", no_comp_stats.total_bytes / (1024.0 * 1024.0));

    free(comp_latencies);
    free(no_comp_latencies);
    ppdb_destroy(store);
    return 0;
}

// 批量操作性能测试
static int benchmark_batch_ops(void) {
    ppdb_kvstore_t* store = NULL;
    int err;
    perf_stats_t batch_stats = {0}, single_stats = {0};
    double* batch_latencies = malloc((TEST_COUNT / BATCH_SIZE) * sizeof(double));
    double* single_latencies = malloc(TEST_COUNT * sizeof(double));
    TEST_ASSERT_NOT_NULL(batch_latencies, "Failed to allocate batch latencies array");
    TEST_ASSERT_NOT_NULL(single_latencies, "Failed to allocate single latencies array");

    // 创建KVStore
    ppdb_config_t config = {
        .enable_wal = true,
        .wal_path = "/tmp/ppdb_batch",
        .sync_write = false
    };
    
    err = ppdb_kvstore_create(&store, &config);
    TEST_ASSERT_OK(err, "Failed to create store");

    // 准备测试数据
    char key[KEY_SIZE];
    char value[SMALL_VALUE_SIZE];
    memset(value, 'v', SMALL_VALUE_SIZE - 1);
    value[SMALL_VALUE_SIZE - 1] = '\0';

    // 批量写入测试
    printf("Testing batch writes...\n");
    for (int batch = 0; batch < TEST_COUNT / BATCH_SIZE; batch++) {
        uint64_t start = get_microseconds();
        
        // 开始批量操作
        err = ppdb_kvstore_batch_begin(store);
        TEST_ASSERT_OK(err, "Failed to begin batch");
        
        for (int i = 0; i < BATCH_SIZE; i++) {
            snprintf(key, sizeof(key), "batch_key_%d_%d", batch, i);
            err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
            TEST_ASSERT_OK(err, "Batch put failed");
            batch_stats.total_bytes += strlen(key) + strlen(value);
        }
        
        // 提交批量操作
        err = ppdb_kvstore_batch_commit(store);
        TEST_ASSERT_OK(err, "Failed to commit batch");
        
        uint64_t end = get_microseconds();
        batch_latencies[batch] = end - start;
    }

    // 单个写入测试
    printf("Testing single writes...\n");
    for (int i = 0; i < TEST_COUNT; i++) {
        snprintf(key, sizeof(key), "single_key_%d", i);
        
        uint64_t start = get_microseconds();
        err = ppdb_kvstore_put(store, key, strlen(key), value, strlen(value));
        uint64_t end = get_microseconds();
        
        TEST_ASSERT_OK(err, "Single put failed");
        single_latencies[i] = end - start;
        single_stats.total_bytes += strlen(key) + strlen(value);
    }

    // 计算统计信息
    calculate_stats(batch_latencies, TEST_COUNT / BATCH_SIZE, &batch_stats);
    calculate_stats(single_latencies, TEST_COUNT, &single_stats);

    // 输出结果
    printf("\nBatch Write Performance (batch size: %d):\n", BATCH_SIZE);
    printf("Throughput: %.2f ops/s\n", batch_stats.throughput * BATCH_SIZE);
    printf("Average Batch Latency: %.2f us\n", batch_stats.avg_latency);
    printf("Average Per-Operation Latency: %.2f us\n", batch_stats.avg_latency / BATCH_SIZE);
    printf("P95 Batch Latency: %.2f us\n", batch_stats.p95_latency);
    printf("P99 Batch Latency: %.2f us\n", batch_stats.p99_latency);
    printf("Total Data Written: %.2f MB\n", batch_stats.total_bytes / (1024.0 * 1024.0));

    printf("\nSingle Write Performance:\n");
    printf("Throughput: %.2f ops/s\n", single_stats.throughput);
    printf("Average Latency: %.2f us\n", single_stats.avg_latency);
    printf("P95 Latency: %.2f us\n", single_stats.p95_latency);
    printf("P99 Latency: %.2f us\n", single_stats.p99_latency);
    printf("Total Data Written: %.2f MB\n", single_stats.total_bytes / (1024.0 * 1024.0));

    free(batch_latencies);
    free(single_latencies);
    ppdb_destroy(store);
    cleanup_test_dir("/tmp/ppdb_batch");
    return 0;
}

// 并发性能测试工作线程
static void* concurrent_worker(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    char key[KEY_SIZE];
    char value[SMALL_VALUE_SIZE];
    memset(value, 'v', SMALL_VALUE_SIZE - 1);
    value[SMALL_VALUE_SIZE - 1] = '\0';
    int err;

    for (size_t i = 0; i < ctx->op_count; i++) {
        snprintf(key, sizeof(key), "key_%d_%zu", ctx->thread_id, i);
        
        uint64_t start = get_microseconds();
        err = ppdb_kvstore_put(ctx->store, key, strlen(key), value, strlen(value));
        uint64_t end = get_microseconds();
        
        if (err == PPDB_OK) {
            ctx->latencies[i] = end - start;
            ctx->stats.total_bytes += strlen(key) + strlen(value);
        }
    }

    return NULL;
}

// 并发性能测试
static int benchmark_concurrent_ops(void) {
    ppdb_kvstore_t* store = NULL;
    int err;
    pthread_t threads[NUM_THREADS];
    thread_context_t contexts[NUM_THREADS];
    size_t ops_per_thread = TEST_COUNT / NUM_THREADS;

    // 创建KVStore
    ppdb_config_t config = {
        .enable_wal = true,
        .wal_path = "/tmp/ppdb_concurrent",
        .sync_write = false,
        .memtable_size = 64 * 1024 * 1024
    };
    
    err = ppdb_kvstore_create(&store, &config);
    TEST_ASSERT_OK(err, "Failed to create store");

    // 创建并启动线程
    printf("Starting %d threads, each performing %zu operations...\n", 
           NUM_THREADS, ops_per_thread);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        contexts[i].store = store;
        contexts[i].thread_id = i;
        contexts[i].op_count = ops_per_thread;
        contexts[i].latencies = malloc(ops_per_thread * sizeof(double));
        memset(&contexts[i].stats, 0, sizeof(perf_stats_t));
        
        TEST_ASSERT_NOT_NULL(contexts[i].latencies, 
                           "Failed to allocate latencies for thread %d", i);
        
        err = pthread_create(&threads[i], NULL, concurrent_worker, &contexts[i]);
        TEST_ASSERT(err == 0, "Failed to create thread %d", i);
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // 合并所有线程的统计信息
    perf_stats_t total_stats = {0};
    double* all_latencies = malloc(TEST_COUNT * sizeof(double));
    TEST_ASSERT_NOT_NULL(all_latencies, "Failed to allocate combined latencies array");
    
    size_t idx = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        for (size_t j = 0; j < ops_per_thread; j++) {
            all_latencies[idx++] = contexts[i].latencies[j];
        }
        total_stats.total_bytes += contexts[i].stats.total_bytes;
    }

    // 计算总体统计信息
    calculate_stats(all_latencies, TEST_COUNT, &total_stats);

    // 输出结果
    printf("\nConcurrent Performance (%d threads):\n", NUM_THREADS);
    printf("Total Throughput: %.2f ops/s\n", total_stats.throughput);
    printf("Average Latency: %.2f us\n", total_stats.avg_latency);
    printf("P95 Latency: %.2f us\n", total_stats.p95_latency);
    printf("P99 Latency: %.2f us\n", total_stats.p99_latency);
    printf("Total Data Written: %.2f MB\n", total_stats.total_bytes / (1024.0 * 1024.0));
    printf("Per-Thread Throughput: %.2f ops/s\n", total_stats.throughput / NUM_THREADS);

    // 清理资源
    for (int i = 0; i < NUM_THREADS; i++) {
        free(contexts[i].latencies);
    }
    free(all_latencies);
    ppdb_destroy(store);
    cleanup_test_dir("/tmp/ppdb_concurrent");
    return 0;
}

// 性能测试套件
static const test_case_t performance_cases[] = {
    {"benchmark_memtable_ops", benchmark_memtable_ops, 120, false, 
     "Benchmark memtable operations (read/write latency and throughput)"},
    {"benchmark_wal_ops", benchmark_wal_ops, 120, false, 
     "Benchmark WAL operations (sync/async write performance)"},
    {"benchmark_compression", benchmark_compression, 120, false,
     "Benchmark compression performance impact"},
    {"benchmark_batch_ops", benchmark_batch_ops, 120, false,
     "Benchmark batch operations vs single operations"},
    {"benchmark_concurrent_ops", benchmark_concurrent_ops, 120, false,
     "Benchmark concurrent operations performance"},
    {NULL, NULL, 0, false, NULL}
};

const test_suite_t performance_suite = {
    .name = "Performance Tests",
    .cases = performance_cases,
    .num_cases = sizeof(performance_cases) / sizeof(performance_cases[0]) - 1,
    .setup = NULL,
    .teardown = NULL
};
