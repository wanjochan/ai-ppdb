#include "ppdb/ppdb.h"
#include "ppdb/internal.h"
#include <cosmopolitan.h>

static void test_skiplist_basic() {
    // 创建基础配置
    ppdb_config_t config = {
        .type = PPDB_TYPE_SKIPLIST,
        .shard_count = 1,
        .use_lockfree = true,
        .memory_limit = 1024 * 1024 * 16,  // 16MB
        .max_key_size = 16 * 1024,    // 16KB
        .max_value_size = 64 * 1024,  // 64KB
        .max_level = MAX_SKIPLIST_LEVEL
    };

    // 创建基础存储
    ppdb_base_t* base;
    ppdb_error_t err = ppdb_create(&base, &config);
    assert(err == PPDB_OK);
    assert(base != NULL);

    // 创建头节点
    ppdb_node_t* head = node_create(base, NULL, NULL, MAX_SKIPLIST_LEVEL);
    assert(head != NULL);
    assert(node_get_height(head) == MAX_SKIPLIST_LEVEL);
    
    // 准备测试数据
    uint8_t* key_data = PPDB_ALIGNED_ALLOC(16);
    uint8_t* value_data = PPDB_ALIGNED_ALLOC(16);
    assert(key_data != NULL);
    assert(value_data != NULL);
    memcpy(key_data, "test_key", 8);
    memcpy(value_data, "test_value", 10);
    
    ppdb_key_t key = {
        .data = key_data,
        .size = 8
    };
    ppdb_value_t value = {
        .data = value_data,
        .size = 10
    };
    
    // 创建测试节点
    ppdb_node_t* node = node_create(base, &key, &value, 4);
    assert(node != NULL);
    assert(node_get_height(node) == 4);
    
    // 测试基本链接
    head->next[0] = node;
    assert(head->next[0] == node);
    
    // 测试节点数据
    assert(node->key->size == key.size);
    assert(memcmp(node->key->data, key.data, key.size) == 0);
    assert(node->value->size == value.size);
    assert(memcmp(node->value->data, value.data, value.size) == 0);
    
    // 清理
    head->next[0] = NULL;  // 断开链接
    node_unref(node);      // 释放测试节点
    node_unref(head);      // 释放头节点
    PPDB_ALIGNED_FREE(key_data);
    PPDB_ALIGNED_FREE(value_data);
    ppdb_destroy(base);    // 销毁基础存储
}

static void test_skiplist_atomic_ops() {
    ppdb_config_t config = {
        .type = PPDB_TYPE_SKIPLIST,
        .shard_count = 1,
        .use_lockfree = true,
        .memory_limit = 1024 * 1024 * 16,  // 16MB
        .max_key_size = 16 * 1024,    // 16KB
        .max_value_size = 64 * 1024,  // 64KB
        .max_level = MAX_SKIPLIST_LEVEL
    };

    ppdb_base_t* base;
    ppdb_error_t err = ppdb_create(&base, &config);
    assert(err == PPDB_OK);

    // 使用静态数据避免内存管理问题
    uint8_t* atomic_key_data = PPDB_ALIGNED_ALLOC(16);
    uint8_t* atomic_value_data = PPDB_ALIGNED_ALLOC(16);
    assert(atomic_key_data != NULL);
    assert(atomic_value_data != NULL);
    memcpy(atomic_key_data, "atomic_key", 10);
    memcpy(atomic_value_data, "atomic_value", 12);
    
    ppdb_key_t key = {
        .data = atomic_key_data,
        .size = 10
    };
    ppdb_value_t value = {
        .data = atomic_value_data,
        .size = 12
    };

    ppdb_node_t* node = node_create(base, &key, &value, 4);
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
    PPDB_ALIGNED_FREE(atomic_key_data);
    PPDB_ALIGNED_FREE(atomic_value_data);
    ppdb_destroy(base);
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
    printf("\n=== PPDB Skiplist Node Test Suite ===\n");
    test_skiplist_basic();
    test_skiplist_atomic_ops();
    test_skiplist_random_level();
    printf("All skiplist node tests passed!\n");
    return 0;
}
