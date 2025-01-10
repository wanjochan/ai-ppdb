#include "test/test_common.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_struct.h"

// 性能统计结构
typedef struct {
    int64_t total_ops;
    int64_t total_time_ns;
    double avg_time_ns;
    double ops_per_sec;
} perf_stats_t;

static perf_stats_t g_list_stats = {0};
static perf_stats_t g_hash_stats = {0};
static perf_stats_t g_rbtree_stats = {0};

// 链表基本测试
static int test_list_basic(void) {
    infra_printf("Running basic list tests...\n");

    ppdb_list_t* list;
    ppdb_error_t err;

    // 创建链表
    err = ppdb_list_create(&list);
    TEST_ASSERT(err == PPDB_OK, "List creation failed");

    // 测试插入
    for (int i = 0; i < 10; i++) {
        int* value = infra_malloc(sizeof(int));
        *value = i;
        err = ppdb_list_append(list, value);
        TEST_ASSERT(err == PPDB_OK, "List append failed");
    }

    // 测试遍历
    ppdb_list_node_t* node = ppdb_list_head(list);
    int i = 0;
    while (node) {
        int* value = (int*)ppdb_list_node_value(node);
        TEST_ASSERT(*value == i, "List traversal failed");
        i++;
        node = ppdb_list_node_next(node);
    }

    // 测试删除
    node = ppdb_list_head(list);
    while (node) {
        ppdb_list_node_t* next = ppdb_list_node_next(node);
        int* value = (int*)ppdb_list_node_value(node);
        ppdb_list_remove(list, node);
        infra_free(value);
        node = next;
    }

    // 销毁链表
    ppdb_list_destroy(list);

    infra_printf("Basic list tests passed\n");
    return 0;
}

// 哈希表基本测试
static int test_hash_basic(void) {
    infra_printf("Running basic hash table tests...\n");

    ppdb_hash_t* hash;
    ppdb_error_t err;

    // 创建哈希表
    err = ppdb_hash_create(&hash, 16);
    TEST_ASSERT(err == PPDB_OK, "Hash table creation failed");

    // 测试插入
    for (int i = 0; i < 100; i++) {
        char key[32];
        infra_snprintf(key, sizeof(key), "key%d", i);
        int* value = infra_malloc(sizeof(int));
        *value = i;
        err = ppdb_hash_put(hash, key, value);
        TEST_ASSERT(err == PPDB_OK, "Hash put failed");
    }

    // 测试查找
    for (int i = 0; i < 100; i++) {
        char key[32];
        infra_snprintf(key, sizeof(key), "key%d", i);
        int* value = ppdb_hash_get(hash, key);
        TEST_ASSERT(value != NULL && *value == i, "Hash get failed");
    }

    // 测试删除
    for (int i = 0; i < 100; i++) {
        char key[32];
        infra_snprintf(key, sizeof(key), "key%d", i);
        int* value = ppdb_hash_remove(hash, key);
        TEST_ASSERT(value != NULL && *value == i, "Hash remove failed");
        infra_free(value);
    }

    // 销毁哈希表
    ppdb_hash_destroy(hash);

    infra_printf("Basic hash table tests passed\n");
    return 0;
}

// 红黑树基本测试
static int test_rbtree_basic(void) {
    infra_printf("Running basic red-black tree tests...\n");

    ppdb_rbtree_t* tree;
    ppdb_error_t err;

    // 创建红黑树
    err = ppdb_rbtree_create(&tree);
    TEST_ASSERT(err == PPDB_OK, "RB-tree creation failed");

    // 测试插入
    for (int i = 0; i < 100; i++) {
        int* value = infra_malloc(sizeof(int));
        *value = i;
        err = ppdb_rbtree_insert(tree, i, value);
        TEST_ASSERT(err == PPDB_OK, "RB-tree insert failed");
    }

    // 测试查找
    for (int i = 0; i < 100; i++) {
        int* value = ppdb_rbtree_find(tree, i);
        TEST_ASSERT(value != NULL && *value == i, "RB-tree find failed");
    }

    // 测试删除
    for (int i = 0; i < 100; i++) {
        int* value = ppdb_rbtree_remove(tree, i);
        TEST_ASSERT(value != NULL && *value == i, "RB-tree remove failed");
        infra_free(value);
    }

    // 销毁红黑树
    ppdb_rbtree_destroy(tree);

    infra_printf("Basic red-black tree tests passed\n");
    return 0;
}

// 链表性能测试
static int test_list_performance(void) {
    infra_printf("Running list performance tests...\n");

    ppdb_list_t* list;
    ppdb_error_t err;
    int64_t start_time, end_time;
    const int NUM_OPS = 10000;

    err = ppdb_list_create(&list);
    TEST_ASSERT(err == PPDB_OK, "List creation failed");

    // 测试插入性能
    start_time = infra_get_time_us();
    for (int i = 0; i < NUM_OPS; i++) {
        int* value = infra_malloc(sizeof(int));
        *value = i;
        err = ppdb_list_append(list, value);
        TEST_ASSERT(err == PPDB_OK, "List append failed");
    }
    end_time = infra_get_time_us();

    g_list_stats.total_ops += NUM_OPS;
    g_list_stats.total_time_ns += (end_time - start_time) * 1000;
    g_list_stats.avg_time_ns = g_list_stats.total_time_ns / g_list_stats.total_ops;
    g_list_stats.ops_per_sec = 1e9 * g_list_stats.total_ops / g_list_stats.total_time_ns;

    infra_printf("List append rate: %.2f ops/sec\n", g_list_stats.ops_per_sec);

    // 清理
    ppdb_list_node_t* node = ppdb_list_head(list);
    while (node) {
        ppdb_list_node_t* next = ppdb_list_node_next(node);
        int* value = (int*)ppdb_list_node_value(node);
        ppdb_list_remove(list, node);
        infra_free(value);
        node = next;
    }
    ppdb_list_destroy(list);

    infra_printf("List performance tests passed\n");
    return 0;
}

// 哈希表性能测试
static int test_hash_performance(void) {
    infra_printf("Running hash table performance tests...\n");

    ppdb_hash_t* hash;
    ppdb_error_t err;
    int64_t start_time, end_time;
    const int NUM_OPS = 100000;

    err = ppdb_hash_create(&hash, 1024);
    TEST_ASSERT(err == PPDB_OK, "Hash table creation failed");

    // 测试插入性能
    start_time = infra_get_time_us();
    for (int i = 0; i < NUM_OPS; i++) {
        char key[32];
        infra_snprintf(key, sizeof(key), "key%d", i);
        int* value = infra_malloc(sizeof(int));
        *value = i;
        err = ppdb_hash_put(hash, key, value);
        TEST_ASSERT(err == PPDB_OK, "Hash put failed");
    }
    end_time = infra_get_time_us();

    g_hash_stats.total_ops += NUM_OPS;
    g_hash_stats.total_time_ns += (end_time - start_time) * 1000;
    g_hash_stats.avg_time_ns = g_hash_stats.total_time_ns / g_hash_stats.total_ops;
    g_hash_stats.ops_per_sec = 1e9 * g_hash_stats.total_ops / g_hash_stats.total_time_ns;

    infra_printf("Hash put rate: %.2f ops/sec\n", g_hash_stats.ops_per_sec);

    // 清理
    ppdb_hash_clear(hash);
    ppdb_hash_destroy(hash);

    infra_printf("Hash table performance tests passed\n");
    return 0;
}

// 红黑树性能测试
static int test_rbtree_performance(void) {
    infra_printf("Running red-black tree performance tests...\n");

    ppdb_rbtree_t* tree;
    ppdb_error_t err;
    int64_t start_time, end_time;
    const int NUM_OPS = 100000;

    err = ppdb_rbtree_create(&tree);
    TEST_ASSERT(err == PPDB_OK, "RB-tree creation failed");

    // 测试插入性能
    start_time = infra_get_time_us();
    for (int i = 0; i < NUM_OPS; i++) {
        int* value = infra_malloc(sizeof(int));
        *value = i;
        err = ppdb_rbtree_insert(tree, i, value);
        TEST_ASSERT(err == PPDB_OK, "RB-tree insert failed");
    }
    end_time = infra_get_time_us();

    g_rbtree_stats.total_ops += NUM_OPS;
    g_rbtree_stats.total_time_ns += (end_time - start_time) * 1000;
    g_rbtree_stats.avg_time_ns = g_rbtree_stats.total_time_ns / g_rbtree_stats.total_ops;
    g_rbtree_stats.ops_per_sec = 1e9 * g_rbtree_stats.total_ops / g_rbtree_stats.total_time_ns;

    infra_printf("RB-tree insert rate: %.2f ops/sec\n", g_rbtree_stats.ops_per_sec);

    // 清理
    ppdb_rbtree_clear(tree);
    ppdb_rbtree_destroy(tree);

    infra_printf("Red-black tree performance tests passed\n");
    return 0;
}

// 打印性能统计信息
static void print_perf_stats(void) {
    infra_printf("\n=== Performance Statistics ===\n");
    infra_printf("List Operations:\n");
    infra_printf("  Total ops: %ld\n", g_list_stats.total_ops);
    infra_printf("  Avg time: %.2f ns\n", g_list_stats.avg_time_ns);
    infra_printf("  Throughput: %.2f ops/sec\n", g_list_stats.ops_per_sec);

    infra_printf("\nHash Table Operations:\n");
    infra_printf("  Total ops: %ld\n", g_hash_stats.total_ops);
    infra_printf("  Avg time: %.2f ns\n", g_hash_stats.avg_time_ns);
    infra_printf("  Throughput: %.2f ops/sec\n", g_hash_stats.ops_per_sec);

    infra_printf("\nRed-Black Tree Operations:\n");
    infra_printf("  Total ops: %ld\n", g_rbtree_stats.total_ops);
    infra_printf("  Avg time: %.2f ns\n", g_rbtree_stats.avg_time_ns);
    infra_printf("  Throughput: %.2f ops/sec\n", g_rbtree_stats.ops_per_sec);
    infra_printf("===========================\n\n");
}

int main(void) {
    int result = 0;
    
    // 运行基本功能测试
    result |= test_list_basic();
    result |= test_hash_basic();
    result |= test_rbtree_basic();

    // 运行性能测试
    result |= test_list_performance();
    result |= test_hash_performance();
    result |= test_rbtree_performance();

    // 打印性能统计
    print_perf_stats();

        return result;
} 