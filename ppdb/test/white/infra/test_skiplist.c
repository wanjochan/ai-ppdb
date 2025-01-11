#include "test_common.h"
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

int main(void) {
    // 初始化infra系统
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }

    TEST_INIT();
    
    TEST_RUN(test_skiplist_basic);
    
    TEST_CLEANUP();

    // 清理infra系统
    infra_cleanup();
    return 0;
}
