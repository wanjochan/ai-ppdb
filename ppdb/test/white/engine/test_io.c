/*
 * test_io.c - Engine IO management tests
 */

#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"
#include "test_common.h"

// Test IO initialization
static void test_io_init(void) {
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

    // Initialize IO management
    err = ppdb_engine_io_init(engine);
    ASSERT_OK(err);

    // Verify IO manager state
    ASSERT_TRUE(engine->io_mgr.io_running);
    ASSERT_NULL(engine->io_mgr.io_mgr);  // Not implemented yet
    ASSERT_NULL(engine->io_mgr.io_thread);  // Not implemented yet

    // Cleanup
    ppdb_engine_destroy(engine);
    ppdb_base_destroy(base);
}

// Test IO cleanup
static void test_io_cleanup(void) {
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

    // Initialize IO management
    err = ppdb_engine_io_init(engine);
    ASSERT_OK(err);

    // Cleanup IO management
    ppdb_engine_io_cleanup(engine);

    // Verify IO manager state
    ASSERT_FALSE(engine->io_mgr.io_running);
    ASSERT_NULL(engine->io_mgr.io_mgr);
    ASSERT_NULL(engine->io_mgr.io_thread);

    // Cleanup
    ppdb_engine_destroy(engine);
    ppdb_base_destroy(base);
}

// Test suite
int main(void) {
    printf("Running test suite: IO Tests\n");

    RUN_TEST(test_io_init);
    RUN_TEST(test_io_cleanup);

    printf("Test suite completed\n");
    return 0;
} 