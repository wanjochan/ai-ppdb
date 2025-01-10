#include "cosmopolitan.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra.h"
#include "test/test_framework.h"

static int test_count = 0;
static int fail_count = 0;

static int test_infra_init(void) {
    printf("Testing infra initialization...\n");
    fflush(stdout);

    // Test memory management
    void* ptr = infra_malloc(42);
    TEST_ASSERT(ptr != NULL, "Memory allocation failed");
    infra_free(ptr);

    // Test string operations
    const char* test_str = "Hello, World!";
    char* str_copy = infra_strdup(test_str);
    TEST_ASSERT(str_copy != NULL, "String duplication failed");
    TEST_ASSERT(infra_strcmp(test_str, str_copy) == 0, "String comparison failed");
    infra_free(str_copy);

    // Test buffer operations
    infra_buffer_t buf;
    TEST_ASSERT(infra_buffer_init(&buf, 64) == INFRA_OK, "Buffer initialization failed");
    const char* test_data = "test data";
    TEST_ASSERT(infra_buffer_write(&buf, test_data, infra_strlen(test_data)) == INFRA_OK, "Buffer write failed");
    char read_data[64];
    TEST_ASSERT(infra_buffer_read(&buf, read_data, infra_strlen(test_data)) == INFRA_OK, "Buffer read failed");
    TEST_ASSERT(infra_memcmp(test_data, read_data, infra_strlen(test_data)) == 0, "Buffer data mismatch");
    infra_buffer_destroy(&buf);

    printf("Infra initialization test passed\n");
    fflush(stdout);
    return 0;
}

static int test_main(void) {
    printf("Running infra tests...\n");
    fflush(stdout);
    
    int result = 0;
    result |= test_infra_init();

    printf("Test completed with result: %d\n", result);
    printf("Total tests: %d, Failed: %d\n", test_count, fail_count);
    printf("Test %s\n", result == 0 ? "PASSED" : "FAILED");
    fflush(stdout);
    return result;
}

COSMOPOLITAN_C_START_
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return test_main();
}
COSMOPOLITAN_C_END_ 