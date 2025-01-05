#include <cosmopolitan.h>
#include <ppdb/internal.h>

// Test macros
#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "Assertion failed: %s\n", #cond); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            fprintf(stderr, "Assertion failed: %s != %s\n", #a, #b); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            fprintf(stderr, "Assertion failed: %s == %s\n", #a, #b); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "Assertion failed: %s is NULL\n", #ptr); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "Assertion failed: %s is not NULL\n", #ptr); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_OK(err) \
    do { \
        if ((err) != PPDB_OK) { \
            fprintf(stderr, "Assertion failed: %s is not OK\n", #err); \
            exit(1); \
        } \
    } while (0)

#define TEST_SUITE_BEGIN(name) \
    do { \
        printf("Running test suite: %s\n", name); \
    } while (0)

#define TEST_RUN(test) \
    do { \
        printf("  Running test: %s\n", #test); \
        test(); \
        printf("  Test passed: %s\n", #test); \
    } while (0)

#define TEST_SUITE_END() \
    do { \
        printf("Test suite completed\n"); \
    } while (0)

// Test cases
void test_error_codes(void) {
    ASSERT_EQ(PPDB_OK, 0);
    ASSERT_NE(PPDB_ERR_OUT_OF_MEMORY, PPDB_OK);
    ASSERT_NE(PPDB_ERR_INVALID_ARGUMENT, PPDB_OK);
    ASSERT_NE(PPDB_ERR_INVALID_STATE, PPDB_OK);
}

void test_error_strings(void) {
    const char* ok_str = ppdb_error_string(PPDB_OK);
    const char* oom_str = ppdb_error_string(PPDB_ERR_OUT_OF_MEMORY);
    const char* invalid_arg_str = ppdb_error_string(PPDB_ERR_INVALID_ARGUMENT);
    const char* invalid_state_str = ppdb_error_string(PPDB_ERR_INVALID_STATE);
    const char* unknown_str = ppdb_error_string(-1);

    ASSERT_NOT_NULL(ok_str);
    ASSERT_NOT_NULL(oom_str);
    ASSERT_NOT_NULL(invalid_arg_str);
    ASSERT_NOT_NULL(invalid_state_str);
    ASSERT_NOT_NULL(unknown_str);

    ASSERT(strcmp(ok_str, "Success") == 0);
    ASSERT(strcmp(oom_str, "Out of memory") == 0);
    ASSERT(strcmp(invalid_arg_str, "Invalid argument") == 0);
    ASSERT(strcmp(invalid_state_str, "Invalid state") == 0);
    ASSERT(strcmp(unknown_str, "Unknown error") == 0);
}

void test_error_propagation(void) {
    ppdb_error_t err;
    void* ptr;

    // Test error propagation through memory allocation
    ptr = ppdb_aligned_alloc(0, 1024);  // Invalid alignment
    ASSERT_NULL(ptr);

    ptr = ppdb_aligned_alloc(16, 0);  // Invalid size
    ASSERT_NULL(ptr);

    // Test error propagation through memory pool
    ppdb_mempool_t* pool = NULL;
    err = ppdb_mempool_create(&pool, 0, 0);  // Invalid arguments
    ASSERT_EQ(err, PPDB_ERR_INVALID_ARGUMENT);
    ASSERT_NULL(pool);
}

int main(int argc, char* argv[]) {
    (void)argc;  // Unused parameter
    (void)argv;  // Unused parameter

    TEST_SUITE_BEGIN("Error Tests");

    TEST_RUN(test_error_codes);
    TEST_RUN(test_error_strings);
    TEST_RUN(test_error_propagation);

    TEST_SUITE_END();
    return 0;
} 