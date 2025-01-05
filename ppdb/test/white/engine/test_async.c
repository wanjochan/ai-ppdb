#include "base/engine/async.h"
#include "base/common/test_common.h"

// ... existing test setup code ...

void test_async_basic() {
    base_async_t *async = base_async_create();
    ASSERT_NOT_NULL(async);
    
    base_async_start(async);
    ASSERT_TRUE(base_async_is_running(async));
    
    base_async_stop(async);
    ASSERT_FALSE(base_async_is_running(async));
    
    base_async_destroy(async);
}

// ... additional test cases ...

int main(int argc, char **argv) {
    test_async_basic();
    // ... other test case calls ...
    return 0;
}