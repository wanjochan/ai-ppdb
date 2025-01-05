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
void test_memory_basic(void) {
    // Test aligned allocation
    void* ptr = ppdb_aligned_alloc(16, 1024);
    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ((uintptr_t)ptr % 16, 0);

    // Write some data
    memset(ptr, 0xAA, 1024);

    // Free memory
    ppdb_aligned_free(ptr);

    // Test invalid parameters
    ptr = ppdb_aligned_alloc(0, 1024);
    ASSERT_NULL(ptr);

    ptr = ppdb_aligned_alloc(16, 0);
    ASSERT_NULL(ptr);
}

void test_memory_pool(void) {
    ppdb_mempool_t* pool = NULL;
    ppdb_error_t err;

    // Create pool
    err = ppdb_mempool_create(&pool, 4096, 16);
    ASSERT_OK(err);
    ASSERT_NOT_NULL(pool);

    // Allocate memory
    void* ptr1 = ppdb_mempool_alloc(pool);
    ASSERT_NOT_NULL(ptr1);
    ASSERT_EQ((uintptr_t)ptr1 % 16, 0);

    void* ptr2 = ppdb_mempool_alloc(pool);
    ASSERT_NOT_NULL(ptr2);
    ASSERT_EQ((uintptr_t)ptr2 % 16, 0);

    // Write some data
    memset(ptr1, 0xAA, 16);
    memset(ptr2, 0xBB, 16);

    // Free memory
    ppdb_mempool_free(pool, ptr1);
    ppdb_mempool_free(pool, ptr2);

    // Destroy pool
    ppdb_mempool_destroy(pool);
}

#define ALLOC_SIZE 1024
#define NUM_ALLOCS 100

static void* thread_func(void* arg) {
    (void)arg;  // Unused parameter
    void* ptrs[NUM_ALLOCS];

    for (int i = 0; i < NUM_ALLOCS; i++) {
        ptrs[i] = ppdb_aligned_alloc(16, ALLOC_SIZE);
        ASSERT_NOT_NULL(ptrs[i]);
        ASSERT_EQ((uintptr_t)ptrs[i] % 16, 0);
        memset(ptrs[i], i & 0xFF, ALLOC_SIZE);
    }

    for (int i = 0; i < NUM_ALLOCS; i++) {
        ppdb_aligned_free(ptrs[i]);
    }

    return NULL;
}

void test_memory_concurrent(void) {
    pthread_t threads[4];

    // Create threads
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_func, NULL);
    }

    // Wait for threads
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;  // Unused parameter
    (void)argv;  // Unused parameter

    TEST_SUITE_BEGIN("Memory Tests");

    TEST_RUN(test_memory_basic);
    TEST_RUN(test_memory_pool);
    TEST_RUN(test_memory_concurrent);

    TEST_SUITE_END();
    return 0;
} 