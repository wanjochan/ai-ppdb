#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"
#include "test_common.h"

// Test suite for transaction management
static void test_txn_basic(void) {
    ppdb_base_t* base = NULL;
    ppdb_engine_t* engine = NULL;
    ppdb_base_config_t config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    PPDB_UNUSED(config);

    // Initialize base layer
    assert(ppdb_base_init(&base, &config) == PPDB_OK);
    assert(base != NULL);

    // Initialize engine layer
    assert(ppdb_engine_init(&engine, base) == PPDB_OK);
    assert(engine != NULL);

    // Get initial stats
    ppdb_engine_stats_t stats;
    ppdb_engine_get_stats(engine, &stats);
    assert(ppdb_base_counter_get(stats.total_txns) == 0);
    assert(ppdb_base_counter_get(stats.active_txns) == 0);

    // Begin a transaction
    ppdb_engine_txn_t* txn = NULL;
    assert(ppdb_engine_txn_begin(engine, &txn) == PPDB_OK);
    assert(txn != NULL);

    // Check transaction stats
    ppdb_engine_txn_stats_t txn_stats;
    ppdb_engine_txn_get_stats(txn, &txn_stats);
    assert(txn_stats.is_active == true);
    assert(txn_stats.is_committed == false);
    assert(txn_stats.is_rolledback == false);
    assert(ppdb_base_counter_get(txn_stats.reads) == 0);
    assert(ppdb_base_counter_get(txn_stats.writes) == 0);

    // Check engine stats after begin
    ppdb_engine_get_stats(engine, &stats);
    assert(ppdb_base_counter_get(stats.total_txns) == 1);
    assert(ppdb_base_counter_get(stats.active_txns) == 1);

    // Commit the transaction
    assert(ppdb_engine_txn_commit(txn) == PPDB_OK);

    // Check transaction stats after commit
    ppdb_engine_txn_get_stats(txn, &txn_stats);
    assert(txn_stats.is_active == false);
    assert(txn_stats.is_committed == true);
    assert(txn_stats.is_rolledback == false);

    // Check engine stats after commit
    ppdb_engine_get_stats(engine, &stats);
    assert(ppdb_base_counter_get(stats.total_txns) == 1);
    assert(ppdb_base_counter_get(stats.active_txns) == 0);

    // Cleanup
    ppdb_engine_destroy(engine);
    ppdb_base_destroy(base);
}

static void test_txn_rollback(void) {
    ppdb_base_t* base = NULL;
    ppdb_engine_t* engine = NULL;
    ppdb_base_config_t config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    PPDB_UNUSED(config);

    // Initialize
    assert(ppdb_base_init(&base, &config) == PPDB_OK);
    assert(ppdb_engine_init(&engine, base) == PPDB_OK);

    // Begin a transaction
    ppdb_engine_txn_t* txn = NULL;
    assert(ppdb_engine_txn_begin(engine, &txn) == PPDB_OK);

    // Check initial stats
    ppdb_engine_stats_t stats;
    ppdb_engine_get_stats(engine, &stats);
    assert(ppdb_base_counter_get(stats.total_txns) == 1);
    assert(ppdb_base_counter_get(stats.active_txns) == 1);

    // Rollback the transaction
    assert(ppdb_engine_txn_rollback(txn) == PPDB_OK);

    // Check transaction stats after rollback
    ppdb_engine_txn_stats_t txn_stats;
    ppdb_engine_txn_get_stats(txn, &txn_stats);
    assert(txn_stats.is_active == false);
    assert(txn_stats.is_committed == false);
    assert(txn_stats.is_rolledback == true);

    // Check engine stats after rollback
    ppdb_engine_get_stats(engine, &stats);
    assert(ppdb_base_counter_get(stats.total_txns) == 1);
    assert(ppdb_base_counter_get(stats.active_txns) == 0);

    // Cleanup
    ppdb_engine_destroy(engine);
    ppdb_base_destroy(base);
}

static void test_txn_error(void) {
    ppdb_base_t* base = NULL;
    ppdb_engine_t* engine = NULL;
    ppdb_base_config_t config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    PPDB_UNUSED(config);

    // Test invalid parameters
    assert(ppdb_engine_init(NULL, NULL) == PPDB_ERR_PARAM);
    assert(ppdb_engine_init(&engine, NULL) == PPDB_ERR_PARAM);
    assert(ppdb_engine_txn_begin(NULL, NULL) == PPDB_ERR_PARAM);

    // Initialize properly
    assert(ppdb_base_init(&base, &config) == PPDB_OK);
    assert(ppdb_engine_init(&engine, base) == PPDB_OK);

    // Begin a transaction
    ppdb_engine_txn_t* txn = NULL;
    assert(ppdb_engine_txn_begin(engine, &txn) == PPDB_OK);
    PPDB_UNUSED(txn);

    // Try to commit twice
    assert(ppdb_engine_txn_commit(txn) == PPDB_OK);
    assert(ppdb_engine_txn_commit(txn) == PPDB_ENGINE_ERR_TXN);

    // Try to rollback after commit
    assert(ppdb_engine_txn_rollback(txn) == PPDB_ENGINE_ERR_TXN);

    // Begin another transaction
    ppdb_engine_txn_t* txn2 = NULL;
    assert(ppdb_engine_txn_begin(engine, &txn2) == PPDB_OK);
    PPDB_UNUSED(txn2);

    // Try to rollback twice
    assert(ppdb_engine_txn_rollback(txn2) == PPDB_OK);
    assert(ppdb_engine_txn_rollback(txn2) == PPDB_ENGINE_ERR_TXN);

    // Try to commit after rollback
    assert(ppdb_engine_txn_commit(txn2) == PPDB_ENGINE_ERR_TXN);

    // Cleanup
    ppdb_engine_destroy(engine);
    ppdb_base_destroy(base);
}

static void test_txn_concurrent(void) {
    ppdb_base_t* base = NULL;
    ppdb_engine_t* engine = NULL;
    ppdb_base_config_t config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    PPDB_UNUSED(config);

    // Initialize with thread safety enabled
    assert(ppdb_base_init(&base, &config) == PPDB_OK);
    assert(ppdb_engine_init(&engine, base) == PPDB_OK);

    // Begin multiple transactions
    ppdb_engine_txn_t* txn1 = NULL;
    ppdb_engine_txn_t* txn2 = NULL;
    ppdb_engine_txn_t* txn3 = NULL;
    PPDB_UNUSED(txn1);
    PPDB_UNUSED(txn2);
    PPDB_UNUSED(txn3);

    assert(ppdb_engine_txn_begin(engine, &txn1) == PPDB_OK);
    assert(ppdb_engine_txn_begin(engine, &txn2) == PPDB_OK);
    assert(ppdb_engine_txn_begin(engine, &txn3) == PPDB_OK);

    // Check engine stats
    ppdb_engine_stats_t stats;
    ppdb_engine_get_stats(engine, &stats);
    assert(ppdb_base_counter_get(stats.total_txns) == 3);
    assert(ppdb_base_counter_get(stats.active_txns) == 3);

    // Commit, rollback, and commit
    assert(ppdb_engine_txn_commit(txn1) == PPDB_OK);
    assert(ppdb_engine_txn_rollback(txn2) == PPDB_OK);
    assert(ppdb_engine_txn_commit(txn3) == PPDB_OK);

    // Check final stats
    ppdb_engine_get_stats(engine, &stats);
    assert(ppdb_base_counter_get(stats.total_txns) == 3);
    assert(ppdb_base_counter_get(stats.active_txns) == 0);

    // Cleanup
    ppdb_engine_destroy(engine);
    ppdb_base_destroy(base);
}

int main(void) {
    printf("Running test suite: Transaction Tests\n");
    
    printf("  Running test: test_txn_basic\n");
    test_txn_basic();
    printf("  Test passed: test_txn_basic\n");
    
    printf("  Running test: test_txn_rollback\n");
    test_txn_rollback();
    printf("  Test passed: test_txn_rollback\n");
    
    printf("  Running test: test_txn_error\n");
    test_txn_error();
    printf("  Test passed: test_txn_error\n");
    
    printf("  Running test: test_txn_concurrent\n");
    test_txn_concurrent();
    printf("  Test passed: test_txn_concurrent\n");
    
    printf("Test suite completed\n");
    return 0;
} 