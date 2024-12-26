#include "test_framework.h"

int run_test_suite(const test_suite_t* suite) {
    ppdb_log_info("Running test suite: %s", suite->name);
    int failed = 0;

    for (int i = 0; i < suite->num_cases; i++) {
        ppdb_log_info("  Running test: %s", suite->cases[i].name);
        int result = suite->cases[i].func();
        if (result != 0) {
            ppdb_log_error("  Test failed: %s", suite->cases[i].name);
            failed++;
        } else {
            ppdb_log_info("  Test passed: %s", suite->cases[i].name);
        }
    }

    if (failed > 0) {
        ppdb_log_error("Test suite %s: %d test(s) failed", suite->name, failed);
    } else {
        ppdb_log_info("Test suite %s: all tests passed", suite->name);
    }

    return failed;
} 