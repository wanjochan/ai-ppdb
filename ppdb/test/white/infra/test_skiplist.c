#include <cosmopolitan.h>
#include "internal/base.h"

// Compare function for integers
static int compare_int(const void* a, const void* b) {
    return (intptr_t)a - (intptr_t)b;
}

// Helper function to verify value
static void verify_value(ppdb_base_skiplist_t* list, intptr_t key, const char* expected) {
    void* actual_value;
    size_t value_size;
    assert(ppdb_base_skiplist_find(list, (void*)key, sizeof(intptr_t), &actual_value, &value_size) == PPDB_OK);
    assert(strcmp((char*)actual_value, expected) == 0);
}

// Basic skiplist test
void test_skiplist_basic(void) {
    ppdb_base_skiplist_t* list;
    assert(ppdb_base_skiplist_create(&list, compare_int) == PPDB_OK);

    // Test empty list
    assert(ppdb_base_skiplist_size(list) == 0);

    // Insert some values
    assert(ppdb_base_skiplist_insert(list, (void*)1, sizeof(intptr_t), "one", strlen("one") + 1) == PPDB_OK);
    assert(ppdb_base_skiplist_insert(list, (void*)2, sizeof(intptr_t), "two", strlen("two") + 1) == PPDB_OK);
    assert(ppdb_base_skiplist_insert(list, (void*)3, sizeof(intptr_t), "three", strlen("three") + 1) == PPDB_OK);

    // Verify values
    verify_value(list, 1, "one");
    verify_value(list, 2, "two");
    verify_value(list, 3, "three");

    // Test non-existent key
    void* value;
    size_t value_size;
    assert(ppdb_base_skiplist_find(list, (void*)4, sizeof(intptr_t), &value, &value_size) != PPDB_OK);

    // Test removal
    assert(ppdb_base_skiplist_remove(list, (void*)2, sizeof(intptr_t)) == PPDB_OK);
    assert(ppdb_base_skiplist_find(list, (void*)2, sizeof(intptr_t), &value, &value_size) != PPDB_OK);

    // Test removing non-existent key
    assert(ppdb_base_skiplist_remove(list, (void*)4, sizeof(intptr_t)) != PPDB_OK);

    // Test size
    assert(ppdb_base_skiplist_size(list) == 2);

    // Cleanup
    ppdb_base_skiplist_destroy(list);
}

// Run all skiplist tests
void run_skiplist_tests(void) {
    printf("Running test suite: Skiplist Tests\n");
    printf("  Running test: test_skiplist_basic\n");
    test_skiplist_basic();
    printf("  Test passed: test_skiplist_basic\n");
    printf("Test suite completed\n");
}
