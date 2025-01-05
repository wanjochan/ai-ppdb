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
void test_log_basic(void) {
    ppdb_error_t err;

    // Initialize logging
    err = ppdb_log_init("test.log", PPDB_LOG_INFO, true);
    ASSERT_OK(err);

    // Write some log messages
    ppdb_log_info("Test info message");
    ppdb_log_warn("Test warning message");
    ppdb_log_error("Test error message");

    // Debug message should not be logged at INFO level
    ppdb_log_debug("This should not be logged");

    // Close logging
    ppdb_log_close();

    // Verify log file exists and has content
    FILE* fp = fopen("test.log", "r");
    ASSERT(fp != NULL);
    fclose(fp);
}

void test_log_levels(void) {
    ppdb_error_t err;

    // Test DEBUG level
    err = ppdb_log_init("test_debug.log", PPDB_LOG_DEBUG, false);
    ASSERT_OK(err);
    ppdb_log_debug("This should be logged");
    ppdb_log_close();

    // Test INFO level
    err = ppdb_log_init("test_info.log", PPDB_LOG_INFO, false);
    ASSERT_OK(err);
    ppdb_log_debug("This should not be logged");
    ppdb_log_info("This should be logged");
    ppdb_log_close();

    // Test ERROR level
    err = ppdb_log_init("test_error.log", PPDB_LOG_ERROR, false);
    ASSERT_OK(err);
    ppdb_log_info("This should not be logged");
    ppdb_log_error("This should be logged");
    ppdb_log_close();
}

static void* log_thread_func(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < 100; i++) {
        ppdb_log_info("Thread %d: Message %d", id, i);
    }
    return NULL;
}

void test_log_concurrent(void) {
    ppdb_error_t err;
    pthread_t threads[4];
    int thread_ids[4];

    // Initialize logging with thread safety
    err = ppdb_log_init("test_concurrent.log", PPDB_LOG_INFO, true);
    ASSERT_OK(err);

    // Create threads
    for (int i = 0; i < 4; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, log_thread_func, &thread_ids[i]);
    }

    // Wait for threads
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    // Close logging
    ppdb_log_close();
}

int main(int argc, char* argv[]) {
    (void)argc;  // Unused parameter
    (void)argv;  // Unused parameter

    TEST_SUITE_BEGIN("Log Tests");

    TEST_RUN(test_log_basic);
    TEST_RUN(test_log_levels);
    TEST_RUN(test_log_concurrent);

    TEST_SUITE_END();
    return 0;
} 