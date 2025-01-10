#include "test/test_common.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_struct.h"
#include "test/test_framework.h"

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
    ppdb_base_skiplist_t list;
    size_t size;
    
    // Initialize skiplist
    assert(ppdb_base_skiplist_init(&list, 4) == PPDB_OK);
    
    // Check initial size
    assert(ppdb_base_skiplist_size(&list, &size) == PPDB_OK);
    assert(size == 0);
    
    // Insert some data
    const char* key1 = "key1";
    const char* value1 = "value1";
    assert(ppdb_base_skiplist_insert(&list, key1, strlen(key1), value1, strlen(value1) + 1) == PPDB_OK);
    
    const char* key2 = "key2";
    const char* value2 = "value2";
    assert(ppdb_base_skiplist_insert(&list, key2, strlen(key2), value2, strlen(value2) + 1) == PPDB_OK);
    
    // Check size after insertions
    assert(ppdb_base_skiplist_size(&list, &size) == PPDB_OK);
    assert(size == 2);
    
    // Cleanup
    assert(ppdb_base_skiplist_destroy(&list) == PPDB_OK);
}

// Run all skiplist tests
void run_skiplist_tests(void) {
    printf("Running test suite: Skiplist Tests\n");
    printf("  Running test: test_skiplist_basic\n");
    test_skiplist_basic();
    printf("  Test passed: test_skiplist_basic\n");
    printf("Test suite completed\n");
}

int main(void) {
    printf("Running test suite: Skiplist Tests\n");
    
    printf("  Running test: test_skiplist_basic\n");
    test_skiplist_basic();
    printf("  Test passed: test_skiplist_basic\n");
    
    printf("Test suite completed\n");
    return 0;
}
