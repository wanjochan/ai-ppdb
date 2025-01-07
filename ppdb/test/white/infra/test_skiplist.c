#include <cosmopolitan.h>
#include "internal/base.h"

// Test suite for skip list
static int __attribute__((used)) compare_int(const void* a, const void* b) {
    return (int)(intptr_t)a - (int)(intptr_t)b;
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
    void* value = NULL;
    assert(ppdb_base_skiplist_find(list, (void*)1, &value) == PPDB_OK);
    assert(strcmp((char*)value, "one") == 0);
    assert(ppdb_base_skiplist_find(list, (void*)2, &value) == PPDB_OK);
    assert(strcmp((char*)value, "two") == 0);
    assert(ppdb_base_skiplist_find(list, (void*)3, &value) == PPDB_OK);
    assert(strcmp((char*)value, "three") == 0);
    assert(ppdb_base_skiplist_find(list, (void*)4, &value) != PPDB_OK);

    // Remove values
    assert(ppdb_base_skiplist_remove(list, (void*)2) == PPDB_OK);
    assert(ppdb_base_skiplist_find(list, (void*)2, &value) != PPDB_OK);

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
