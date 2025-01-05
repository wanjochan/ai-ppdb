#include "base/engine/sync.h"
#include "base/engine/common.h"
#include "base/test/test_common.h"

// ... test setup code ...

void test_sync_basic() {
    base_sync_t *sync = base_sync_create();
    ASSERT_NOT_NULL(sync);
    
    base_sync_destroy(sync);
}

void test_sync_lock() {
    base_sync_t *sync = base_sync_create();
    ASSERT_NOT_NULL(sync);
    
    ASSERT_TRUE(base_sync_lock(sync));
    ASSERT_TRUE(base_sync_unlock(sync));
    
    base_sync_destroy(sync);
}

// ... additional test cases ...

int main(int argc, char **argv) {
    TEST_INIT();
    
    TEST_RUN(test_sync_basic);
    TEST_RUN(test_sync_lock);
    
    TEST_CLEANUP();
    return 0;
}