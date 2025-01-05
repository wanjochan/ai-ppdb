/*
 * test_core.c - Engine core functionality tests
 */

#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"
#include "test_common.h"

// Test engine initialization
static void test_engine_init(void) {
    ppdb_base_t* base = NULL;
    ppdb_engine_t* engine = NULL;
    ppdb_base_config_t base_config = {0};
    ppdb_error_t err;

    // Initialize base layer
    err = ppdb_base_init(&base, &base_config);
    ASSERT_OK(err);

    // Initialize engine layer
    err = ppdb_engine_init(&engine, base);
    ASSERT_OK(err);

    // Verify engine state
    ASSERT_NOT_NULL(engine);
    ASSERT_NOT_NULL(engine->base);
    ASSERT_NOT_NULL(engine->global_mutex);

    // Cleanup
    ppdb_engine_destroy(engine);
    ppdb_base_destroy(base);
}

// Test engine statistics
static void test_engine_stats(void) {
    ppdb_base_t* base = NULL;
    ppdb_engine_t* engine = NULL;
    ppdb_base_config_t base_config = {0};
    ppdb_engine_stats_t stats = {0};
    ppdb_error_t err;

    // Initialize base layer
    err = ppdb_base_init(&base, &base_config);
    ASSERT_OK(err);

    // Initialize engine layer
    err = ppdb_engine_init(&engine, base);
    ASSERT_OK(err);

    // Get initial statistics
    ppdb_engine_get_stats(engine, &stats);
    ASSERT_EQUAL(0, stats.total_txns);
    ASSERT_EQUAL(0, stats.active_txns);
    ASSERT_EQUAL(0, stats.total_reads);
    ASSERT_EQUAL(0, stats.total_writes);

    // Cleanup
    ppdb_engine_destroy(engine);
    ppdb_base_destroy(base);
}

// Test engine error handling
static void test_engine_error(void) {
    ppdb_engine_t* engine = NULL;
    ppdb_error_t err;

    // Test invalid parameters
    err = ppdb_engine_init(NULL, NULL);
    ASSERT_EQUAL(PPDB_ERR_PARAM, err);

    err = ppdb_engine_init(&engine, NULL);
    ASSERT_EQUAL(PPDB_ERR_PARAM, err);

    // Test error messages
    const char* msg = ppdb_engine_strerror(PPDB_ENGINE_ERR_INIT);
    ASSERT_NOT_NULL(msg);
    ASSERT_STR_EQUAL("Engine initialization failed", msg);

    msg = ppdb_engine_strerror(PPDB_ENGINE_ERR_PARAM);
    ASSERT_NOT_NULL(msg);
    ASSERT_STR_EQUAL("Invalid parameter in engine operation", msg);
}

// Test suite
int main(void) {
    printf("Running test suite: Core Tests\n");

    RUN_TEST(test_engine_init);
    RUN_TEST(test_engine_stats);
    RUN_TEST(test_engine_error);

    printf("Test suite completed\n");
    return 0;
} 