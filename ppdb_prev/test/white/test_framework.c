/*
 * test_framework.c - Test Framework Implementation
 */

#include <cosmopolitan.h>
#include "test_framework.h"

// Global test state
static test_state_t g_test_state = {
    .initialized = false,
    .config = {
        .type = TEST_TYPE_ALL
    },
    .stats = {
        .start_time = 0,
        .end_time = 0,
        .peak_memory = 0
    }
};

// Test statistics
static struct {
    int total;
    int passed;
    int failed;
    int skipped;
} g_test_stats = {0};

void test_framework_init(void) {
    if (g_test_state.initialized) return;
    
    g_test_state.initialized = true;
    g_test_state.stats.start_time = clock();
    
    g_test_stats.total = 0;
    g_test_stats.passed = 0;
    g_test_stats.failed = 0;
    g_test_stats.skipped = 0;
}

void test_framework_cleanup(void) {
    if (!g_test_state.initialized) return;
    
    g_test_state.stats.end_time = clock();
    g_test_state.initialized = false;
}

int run_test_case(const test_case_t* test_case) {
    if (!test_case || !test_case->fn) return -1;
    
    g_test_stats.total++;
    
    if (test_case->skip) {
        printf("  Skipping test: %s\n", test_case->name);
        g_test_stats.skipped++;
        return 0;
    }
    
    printf("  Running test: %s\n", test_case->name);
    if (test_case->description) {
        printf("  Description: %s\n", test_case->description);
    }
    
    int result = test_case->fn();
    
    if (result == 0) {
        printf("  Test passed: %s\n", test_case->name);
        g_test_stats.passed++;
    } else {
        printf("  Test failed: %s (error: %d)\n", test_case->name, result);
        g_test_stats.failed++;
    }
    
    return result;
}

int run_test_suite(const test_suite_t* suite) {
    if (!suite) return -1;
    
    printf("\nRunning test suite: %s\n", suite->name);
    
    if (suite->setup) {
        printf("Setting up test suite...\n");
        suite->setup();
    }
    
    int failed = 0;
    for (size_t i = 0; i < suite->num_cases; i++) {
        if (run_test_case(&suite->cases[i]) != 0) {
            failed++;
        }
    }
    
    if (suite->teardown) {
        printf("Cleaning up test suite...\n");
        suite->teardown();
    }
    
    return failed;
}

bool test_framework_should_run(test_type_t type) {
    return (g_test_state.config.type & type) != 0;
}

void test_print_stats(void) {
    double elapsed = ((double)(g_test_state.stats.end_time - g_test_state.stats.start_time)) / CLOCKS_PER_SEC;
    
    printf("\nTest Summary:\n");
    printf("  Total tests: %d\n", g_test_stats.total);
    printf("  Passed: %d\n", g_test_stats.passed);
    printf("  Failed: %d\n", g_test_stats.failed);
    printf("  Skipped: %d\n", g_test_stats.skipped);
    printf("  Time elapsed: %.2f seconds\n", elapsed);
}