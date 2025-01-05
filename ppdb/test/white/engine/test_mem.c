// Note: File moved from white/engine to base/engine

// Update header includes
#include "base/engine/mem.h"
#include "test_common.h"

// ... existing test setup code ...

void test_mem_alloc() {
    void* ptr = base_mem_alloc(100);
    ASSERT_NOT_NULL(ptr);
    
    base_mem_free(ptr);
}

void test_mem_realloc() {
    void* ptr = base_mem_alloc(50);
    ASSERT_NOT_NULL(ptr);
    
    ptr = base_mem_realloc(ptr, 100);
    ASSERT_NOT_NULL(ptr);
    
    base_mem_free(ptr);
}

// ... remaining test cases ...

int main(int argc, char** argv) {
    TEST_INIT();
    
    RUN_TEST(test_mem_alloc);
    RUN_TEST(test_mem_realloc);
    
    // ... other test case runs ...
    
    TEST_REPORT();
    return 0;
}