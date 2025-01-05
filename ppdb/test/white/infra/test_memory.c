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

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "Assertion failed: %s is NULL\n", #ptr); \
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

// Test configuration
#define NUM_THREADS 4
#define NUM_ALLOCS 1000
#define ALLOC_SIZE 1024

// Forward declarations
static void* thread_func(void*);

// Test cases
void test_memory_basic(void) {
    // Test basic allocation
    void* ptr = ppdb_aligned_alloc(1024);
    ASSERT_NOT_NULL(ptr);

    // Test memory access
    memset(ptr, 0x42, 1024);
    for (size_t i = 0; i < 1024; i++) {
        ASSERT_EQ(((uint8_t*)ptr)[i], 0x42);
    }

    // Test deallocation
    ppdb_aligned_free(ptr);
}

static void* thread_func(void* arg) {
    (void)arg;  // Unused parameter
    for (int i = 0; i < NUM_ALLOCS; i++) {
        void* ptr = ppdb_aligned_alloc(ALLOC_SIZE);
        ASSERT_NOT_NULL(ptr);
        memset(ptr, 0x42, ALLOC_SIZE);
        ppdb_aligned_free(ptr);
    }
    return NULL;
}

void test_memory_concurrent(void) {
    pthread_t threads[NUM_THREADS];

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_func, NULL);
    }

    // Wait for threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
}

static int test_main(void) {
    TEST_SUITE_BEGIN("Memory Tests");

    TEST_RUN(test_memory_basic);
    TEST_RUN(test_memory_concurrent);

    TEST_SUITE_END();
    return 0;
}

COSMOPOLITAN_C_START_
int main(int argc, char* argv[]) {
    (void)argc;  // Unused parameter
    (void)argv;  // Unused parameter
    return test_main();
}
COSMOPOLITAN_C_END_ 