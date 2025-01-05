#include "ppdb/ppdb.h"
#include "ppdb/internal.h"
#include "ppdb/internal/base.h"
#include <cosmopolitan.h>

static void test_skiplist_basic() {
    // 创建基础配置
    ppdb_options_t config = {
        .db_path = ":memory:",
        .cache_size = 1024 * 1024 * 16,  // 16MB
        .max_readers = 32,
        .sync_writes = false,
        .flush_period_ms = 1000
    };

    // 创建基础存储
    ppdb_ctx_t ctx;
    ppdb_error_t err = ppdb_create(&ctx, &config);
    assert(err == PPDB_OK);

    // 创建头节点
    ppdb_node_t* head = node_create(NULL, NULL, NULL, MAX_SKIPLIST_LEVEL);
    assert(head != NULL);
    assert(node_get_height(head) == MAX_SKIPLIST_LEVEL);
    
    // 准备测试数据
    ppdb_data_t key = {0};
    ppdb_data_t value = {0};
    
    memcpy(key.inline_data, "test_key", 8);
    key.size = 8;
    key.flags = 0;  // 使用内联存储
    
    memcpy(value.inline_data, "test_value", 10);
    value.size = 10;
    value.flags = 0;  // 使用内联存储
    
    // 创建测试节点
    ppdb_node_t* node = node_create(NULL, &key, &value, 4);
    assert(node != NULL);
    assert(node_get_height(node) == 4);
    
    // 测试基本链接
    head->next[0] = node;
    assert(head->next[0] == node);
    
    // 测试节点数据
    assert(node->key != NULL);
    assert(node->value != NULL);
    assert(node->key->size == key.size);
    assert(node->value->size == value.size);
    assert(memcmp(node->key->inline_data, key.inline_data, key.size) == 0);
    assert(memcmp(node->value->inline_data, value.inline_data, value.size) == 0);
    
    // 清理
    head->next[0] = NULL;  // 断开链接
    node_unref(node);      // 释放测试节点
    node_unref(head);      // 释放头节点
    ppdb_destroy(ctx);    // 销毁基础存储
}

static void test_skiplist_atomic_ops() {
    // 创建基础配置
    ppdb_options_t config = {
        .db_path = ":memory:",
        .cache_size = 1024 * 1024 * 16,  // 16MB
        .max_readers = 32,
        .sync_writes = false,
        .flush_period_ms = 1000
    };

    // 创建基础存储
    ppdb_ctx_t ctx;
    ppdb_error_t err = ppdb_create(&ctx, &config);
    assert(err == PPDB_OK);

    // 准备测试数据
    ppdb_data_t key = {0};
    ppdb_data_t value = {0};
    
    memcpy(key.inline_data, "atomic_key", 10);
    key.size = 10;
    key.flags = 0;  // 使用内联存储
    
    memcpy(value.inline_data, "atomic_value", 12);
    value.size = 12;
    value.flags = 0;  // 使用内联存储

    ppdb_node_t* node = node_create(NULL, &key, &value, 4);
    assert(node != NULL);

    // Test reference counting
    assert(atomic_load(&node->state_machine.ref_count) == 1);
    node_ref(node);  // ref_count = 2
    assert(atomic_load(&node->state_machine.ref_count) == 2);
    node_ref(node);  // ref_count = 3
    assert(atomic_load(&node->state_machine.ref_count) == 3);
    node_unref(node);  // ref_count = 2
    assert(atomic_load(&node->state_machine.ref_count) == 2);
    node_unref(node);  // ref_count = 1
    assert(atomic_load(&node->state_machine.ref_count) == 1);
    
    // Test state transitions
    assert(node_is_active(node));
    assert(node_try_mark(node));
    assert(!node_is_active(node));
    
    // 释放节点
    node_unref(node);  // ref_count = 0，这会触发node的销毁
    ppdb_destroy(ctx);
}

static void test_skiplist_random_level() {
    // Test random level distribution
    int level_counts[MAX_SKIPLIST_LEVEL] = {0};
    const int iterations = 10000;

    for (int i = 0; i < iterations; i++) {
        uint32_t level = random_level();
        assert(level >= 1 && level <= MAX_SKIPLIST_LEVEL);
        level_counts[level - 1]++;
    }

    // Print level distribution
    printf("\nLevel distribution:\n");
    for (int i = 0; i < MAX_SKIPLIST_LEVEL; i++) {
        if (level_counts[i] > 0) {
            printf("Level %2d: %5d nodes", i + 1, level_counts[i]);
            if (i > 0) {
                float ratio = (float)level_counts[i] / level_counts[i-1];
                printf(" (ratio: %.3f)", ratio);
            }
            printf("\n");
        }
    }

    // Verify level distribution follows approximate 1/4 probability
    for (int i = 1; i < MAX_SKIPLIST_LEVEL - 1; i++) {
        if (level_counts[i] == 0 || level_counts[i-1] == 0) continue;
        float ratio = (float)level_counts[i] / level_counts[i-1];
        printf("Level %d to %d ratio: %.3f\n", i + 1, i, ratio);
        // Allow some variance due to randomness
        assert(ratio > 0.15 && ratio < 0.35);
    }
}

int main(int argc, char** argv) {
    (void)argc;  // 未使用的参数
    (void)argv;  // 未使用的参数
    
    printf("\n=== PPDB Skiplist Node Test Suite ===\n");
    test_skiplist_basic();
    test_skiplist_atomic_ops();
    test_skiplist_random_level();
    printf("All skiplist node tests passed!\n");
    return 0;
}
