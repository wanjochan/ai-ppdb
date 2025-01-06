#include "test_framework.h"

// Global test state
static test_state_t g_test_state;

void test_framework_init(void) {
    g_test_state.initialized = true;
    g_test_state.config.type = TEST_TYPE_UNIT;
    g_test_state.stats.start_time = clock();
    g_test_state.stats.peak_memory = 0;
}

void test_framework_cleanup(void) {
    g_test_state.stats.end_time = clock();
    test_print_stats();
}

int run_test_case(const test_case_t* test_case) {
    if (!test_case || !test_case->fn) {
        return -1;
    }

    printf("Running test: %s\n", test_case->name);
    int result = test_case->fn();
    printf("Test %s: %s\n", test_case->name, result == 0 ? "PASSED" : "FAILED");
    return result;
}

int run_test_suite(const test_suite_t* suite) {
    if (!suite) {
        return -1;
    }

    printf("Running test suite: %s\n", suite->name);

    if (suite->setup) {
        suite->setup();
    }

    int failed = 0;
    for (size_t i = 0; i < suite->num_cases; i++) {
        if (!suite->cases[i].skip) {
            if (run_test_case(&suite->cases[i]) != 0) {
                failed++;
            }
        }
    }

    if (suite->teardown) {
        suite->teardown();
    }

    printf("Test suite completed: %zu passed, %d failed\n",
           suite->num_cases - failed, failed);
    return failed;
}

bool test_framework_should_run(test_type_t type) {
    return (g_test_state.config.type & type) != 0;
}

void test_print_stats(void) {
    double elapsed = (double)(g_test_state.stats.end_time - g_test_state.stats.start_time) / CLOCKS_PER_SEC;
    printf("\nTest Statistics:\n");
    printf("  Time: %.2f seconds\n", elapsed);
    printf("  Peak Memory: %zu bytes\n", g_test_state.stats.peak_memory);
}