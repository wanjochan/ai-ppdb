#include <cosmopolitan.h>
#include "internal/base.h"

// Test suite for skip list
static int __attribute__((used)) compare_int(const void* a, const void* b) {
    return (int)(intptr_t)a - (int)(intptr_t)b;
}

static void verify_value(ppdb_base_skiplist_t* list, intptr_t key, const char* expected_value) {
    void* actual_value = NULL;
    assert(ppdb_base_skiplist_find(list, (void*)key, &actual_value) == PPDB_OK);
    assert(actual_value != NULL);
    assert(strcmp((const char*)actual_value, expected_value) == 0);
    (void)actual_value;  // Suppress unused variable warning
    (void)list;  // Suppress unused parameter warning
    (void)key;   // Suppress unused parameter warning
    (void)expected_value;  // Suppress unused parameter warning
}

static void test_skiplist_basic(void) {
    ppdb_base_skiplist_t* list = NULL;

    // Create skip list
    assert(ppdb_base_skiplist_create(&list, compare_int) == PPDB_OK);
    assert(list != NULL);

    // Insert some values
    assert(ppdb_base_skiplist_insert(list, (void*)1, (void*)"one") == PPDB_OK);
    assert(ppdb_base_skiplist_insert(list, (void*)2, (void*)"two") == PPDB_OK);
    assert(ppdb_base_skiplist_insert(list, (void*)3, (void*)"three") == PPDB_OK);

    // Find values
    verify_value(list, 1, "one");
    verify_value(list, 2, "two");
    verify_value(list, 3, "three");
    
    // Test non-existent key
    assert(ppdb_base_skiplist_find(list, (void*)4, NULL) != PPDB_OK);

    // Remove values
    assert(ppdb_base_skiplist_remove(list, (void*)2) == PPDB_OK);
    assert(ppdb_base_skiplist_find(list, (void*)2, NULL) != PPDB_OK);

    // Try to remove non-existent key
    assert(ppdb_base_skiplist_remove(list, (void*)4) != PPDB_OK);

    // Check size
    assert(ppdb_base_skiplist_size(list) == 2);

    // Destroy skip list
    ppdb_base_skiplist_destroy(list);
}

int main(void) {
    printf("Running test suite: Skip List Tests\n");
    
    printf("  Running test: test_skiplist_basic\n");
    test_skiplist_basic();
    printf("  Test passed: test_skiplist_basic\n");
    
    printf("Test suite completed\n");
    return 0;
}
