/*
 * test_framework.c - Test Framework Implementation
 */

#include <cosmopolitan.h>
#include "test_framework.h"

int run_test_case(const test_case_t* test_case) {
    if (!test_case || !test_case->fn) {
        return -1;
    }

    printf("Running test: %s\n", test_case->name);
    test_stats.total_tests++;

    int result = test_case->fn();
    if (result != 0) {
        test_stats.failed_tests++;
        printf("Test %s failed with result: %d\n", test_case->name, result);
    }

    return result;
}