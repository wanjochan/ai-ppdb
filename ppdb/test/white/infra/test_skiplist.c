#include "../../../framework/test_framework.h"
#include "internal/infra/infra.h"
#include "test_framework.h"

// Compare function for integers
static int compare_int(const void* a, const void* b) {
    intptr_t va = *(const intptr_t*)a;
    intptr_t vb = *(const intptr_t*)b;
    return (va > vb) - (va < vb);
}

// Helper function to verify value
static int verify_value(infra_skiplist_t* list, intptr_t key, const char* expected) {
    void* actual_value;
    size_t value_size;
    TEST_ASSERT(infra_skiplist_find(list, &key, sizeof(key), &actual_value, &value_size) == INFRA_OK);
    TEST_ASSERT(infra_strcmp((char*)actual_value, expected) == 0);
    return 0;
}

// Basic skiplist test
static int test_skiplist_basic(void) {
    infra_skiplist_t list;
    size_t size;
    
    // Initialize skiplist
    TEST_ASSERT(infra_skiplist_init(&list, 4) == INFRA_OK);
    list.compare = compare_int;
    
    // Check initial size
    TEST_ASSERT(infra_skiplist_size(&list, &size) == INFRA_OK);
    TEST_ASSERT(size == 0);
    
    // Insert some data
    intptr_t key1 = 1;
    const char* value1 = "value1";
    TEST_ASSERT(infra_skiplist_insert(&list, &key1, sizeof(key1), value1, strlen(value1) + 1) == INFRA_OK);
    
    intptr_t key2 = 2;
    const char* value2 = "value2";
    TEST_ASSERT(infra_skiplist_insert(&list, &key2, sizeof(key2), value2, strlen(value2) + 1) == INFRA_OK);
    
    // Check size after insertions
    TEST_ASSERT(infra_skiplist_size(&list, &size) == INFRA_OK);
    TEST_ASSERT(size == 2);
    
    // Verify values
    TEST_ASSERT(verify_value(&list, key1, value1) == 0);
    TEST_ASSERT(verify_value(&list, key2, value2) == 0);
    
    // Remove a value
    TEST_ASSERT(infra_skiplist_remove(&list, &key1, sizeof(key1)) == INFRA_OK);
    
    // Check size after removal
    TEST_ASSERT(infra_skiplist_size(&list, &size) == INFRA_OK);
    TEST_ASSERT(size == 1);
    
    // Verify remaining value
    TEST_ASSERT(verify_value(&list, key2, value2) == 0);
    
    // Clear the list
    TEST_ASSERT(infra_skiplist_clear(&list) == INFRA_OK);
    
    // Check size after clear
    TEST_ASSERT(infra_skiplist_size(&list, &size) == INFRA_OK);
    TEST_ASSERT(size == 0);
    
    // Cleanup
    TEST_ASSERT(infra_skiplist_destroy(&list) == INFRA_OK);
    
    return 0;
}

// Error handling test
static int test_skiplist_error(void) {
    infra_skiplist_t list;
    void* value;
    size_t value_size;
    intptr_t key = 1;
    
    // Test invalid parameters
    TEST_ASSERT(infra_skiplist_init(NULL, 4) == INFRA_ERROR_INVALID);
    TEST_ASSERT(infra_skiplist_init(&list, 0) == INFRA_ERROR_INVALID);
    TEST_ASSERT(infra_skiplist_init(&list, INFRA_SKIPLIST_MAX_LEVEL + 1) == INFRA_ERROR_INVALID);
    
    // Initialize skiplist
    TEST_ASSERT(infra_skiplist_init(&list, 4) == INFRA_OK);
    
    // Test operations without compare function
    TEST_ASSERT(infra_skiplist_insert(&list, &key, sizeof(key), "value", 6) == INFRA_ERROR_INVALID);
    TEST_ASSERT(infra_skiplist_find(&list, &key, sizeof(key), &value, &value_size) == INFRA_ERROR_INVALID);
    TEST_ASSERT(infra_skiplist_remove(&list, &key, sizeof(key)) == INFRA_ERROR_INVALID);
    
    // Set compare function
    list.compare = compare_int;
    
    // Test not found case
    TEST_ASSERT(infra_skiplist_find(&list, &key, sizeof(key), &value, &value_size) == INFRA_ERROR_NOT_FOUND);
    TEST_ASSERT(infra_skiplist_remove(&list, &key, sizeof(key)) == INFRA_ERROR_NOT_FOUND);
    
    // Cleanup
    TEST_ASSERT(infra_skiplist_destroy(&list) == INFRA_OK);
    
    return 0;
}

// Performance test
static int test_skiplist_performance(void) {
    infra_skiplist_t list;
    size_t size;
    char value_buf[32];
    const int num_items = 1000;
    infra_time_t start, end;
    
    // Initialize skiplist
    TEST_ASSERT(infra_skiplist_init(&list, 16) == INFRA_OK);
    list.compare = compare_int;
    
    // Insert items
    start = infra_time_monotonic();
    for (int i = 0; i < num_items; i++) {
        intptr_t key = i;
        snprintf(value_buf, sizeof(value_buf), "value%d", i);
        TEST_ASSERT(infra_skiplist_insert(&list, &key, sizeof(key), value_buf, strlen(value_buf) + 1) == INFRA_OK);
    }
    end = infra_time_monotonic();
    double insert_time = (double)(end - start) / 1000000.0;
    TEST_ASSERT(insert_time < 1.0); // 插入1000个项应该不超过1秒
    
    // Verify size
    TEST_ASSERT(infra_skiplist_size(&list, &size) == INFRA_OK);
    TEST_ASSERT(size == num_items);
    
    // Find items
    start = infra_time_monotonic();
    for (int i = 0; i < num_items; i++) {
        intptr_t key = i;
        void* value;
        size_t value_size;
        TEST_ASSERT(infra_skiplist_find(&list, &key, sizeof(key), &value, &value_size) == INFRA_OK);
        snprintf(value_buf, sizeof(value_buf), "value%d", i);
        TEST_ASSERT(infra_strcmp((char*)value, value_buf) == 0);
    }
    end = infra_time_monotonic();
    double find_time = (double)(end - start) / 1000000.0;
    TEST_ASSERT(find_time < 1.0); // 查找1000个项应该不超过1秒
    
    // Remove items
    start = infra_time_monotonic();
    for (int i = 0; i < num_items; i++) {
        intptr_t key = i;
        TEST_ASSERT(infra_skiplist_remove(&list, &key, sizeof(key)) == INFRA_OK);
    }
    end = infra_time_monotonic();
    double remove_time = (double)(end - start) / 1000000.0;
    TEST_ASSERT(remove_time < 1.0); // 删除1000个项应该不超过1秒
    
    // Verify empty
    TEST_ASSERT(infra_skiplist_size(&list, &size) == INFRA_OK);
    TEST_ASSERT(size == 0);
    
    // Cleanup
    TEST_ASSERT(infra_skiplist_destroy(&list) == INFRA_OK);
    
    return 0;
}

// Level distribution test
static int test_skiplist_level(void) {
    infra_skiplist_t list;
    const int num_items = 10000;
    int level_counts[INFRA_SKIPLIST_MAX_LEVEL] = {0};
    
    // Initialize skiplist
    TEST_ASSERT(infra_skiplist_init(&list, INFRA_SKIPLIST_MAX_LEVEL) == INFRA_OK);
    list.compare = compare_int;
    
    // Insert items and count levels
    for (int i = 0; i < num_items; i++) {
        intptr_t key = i;
        char value = 'a';
        TEST_ASSERT(infra_skiplist_insert(&list, &key, sizeof(key), &value, sizeof(value)) == INFRA_OK);
    }

    // Count levels
    infra_skiplist_node_t* current = list.header->forward[0];
    while (current) {
        level_counts[current->level - 1]++;
        current = current->forward[0];
    }
    
    // Verify level distribution (大致符合概率分布：每个级别的节点数约为下一级别的1/2)
    // 由于随机性，我们允许较大的误差范围
    for (size_t i = 1; i < 4; i++) {  // 只检查前4层，因为更高层的节点太少，比例不稳定
        if (level_counts[i - 1] > 0) {  // 避免除以0
            double ratio = (double)level_counts[i] / level_counts[i - 1];
            TEST_ASSERT(ratio >= 0.2 && ratio <= 0.8);  // 放宽比例要求
        }
    }
    
    // Cleanup
    TEST_ASSERT(infra_skiplist_destroy(&list) == INFRA_OK);
    
    return 0;
}

int main(void) {
    // 初始化infra系统
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }

    TEST_INIT();
    
    TEST_RUN(test_skiplist_basic);      // 基本功能测试
    TEST_RUN(test_skiplist_error);      // 错误处理测试
    TEST_RUN(test_skiplist_performance);  // 性能测试
    TEST_RUN(test_skiplist_level);      // 层级分布测试
    
    TEST_CLEANUP();

    // 清理infra系统
    infra_cleanup();
    return 0;
}
