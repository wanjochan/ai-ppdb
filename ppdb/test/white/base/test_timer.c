#include <cosmopolitan.h>
#include "internal/base.h"

// Test data
static int timer_count = 0;

// Timer callback
static void test_timer_callback(ppdb_base_timer_t* timer, void* data) {
    timer_count++;
}

// Test timer basic operations
static void test_timer_basic(void) {
    ppdb_error_t err;
    ppdb_base_event_loop_t* loop = NULL;
    ppdb_base_timer_t* timer = NULL;
    ppdb_base_timer_stats_t stats;

    // Create event loop
    err = ppdb_base_event_loop_create(&loop);
    assert(err == PPDB_OK);
    assert(loop != NULL);

    // Create timer
    err = ppdb_base_timer_create(loop, &timer);
    assert(err == PPDB_OK);
    assert(timer != NULL);

    // Check initial stats
    ppdb_base_timer_get_stats(timer, &stats);
    assert(stats.total_timeouts == 0);
    assert(stats.active_timers == 0);
    assert(stats.total_resets == 0);
    assert(stats.total_cancels == 0);

    // Cleanup
    ppdb_base_timer_destroy(timer);
    ppdb_base_event_loop_destroy(loop);
}

// Test timer operations
static void test_timer_operations(void) {
    ppdb_error_t err;
    ppdb_base_event_loop_t* loop = NULL;
    ppdb_base_timer_t* timer = NULL;
    ppdb_base_timer_stats_t stats;

    // Create event loop and timer
    err = ppdb_base_event_loop_create(&loop);
    assert(err == PPDB_OK);
    err = ppdb_base_timer_create(loop, &timer);
    assert(err == PPDB_OK);

    // Test one-shot timer
    timer_count = 0;
    err = ppdb_base_timer_start(timer, 50, false, test_timer_callback, NULL);
    assert(err == PPDB_OK);

    // Run event loop
    err = ppdb_base_event_loop_run(loop, 100);
    assert(err == PPDB_OK);

    // Check results
    assert(timer_count == 1);
    ppdb_base_timer_get_stats(timer, &stats);
    assert(stats.total_timeouts == 1);
    assert(stats.active_timers == 0);

    // Test repeating timer
    timer_count = 0;
    err = ppdb_base_timer_start(timer, 50, true, test_timer_callback, NULL);
    assert(err == PPDB_OK);

    // Run event loop
    err = ppdb_base_event_loop_run(loop, 200);
    assert(err == PPDB_OK);

    // Check results
    assert(timer_count > 1);
    ppdb_base_timer_get_stats(timer, &stats);
    assert(stats.total_timeouts > 1);
    assert(stats.active_timers == 1);

    // Test timer stop
    err = ppdb_base_timer_stop(timer);
    assert(err == PPDB_OK);
    ppdb_base_timer_get_stats(timer, &stats);
    assert(stats.active_timers == 0);
    assert(stats.total_cancels == 1);

    // Cleanup
    ppdb_base_timer_destroy(timer);
    ppdb_base_event_loop_destroy(loop);
}

// Test timer error handling
static void test_timer_errors(void) {
    ppdb_error_t err;
    ppdb_base_event_loop_t* loop = NULL;
    ppdb_base_timer_t* timer = NULL;

    // Test invalid parameters
    err = ppdb_base_timer_create(NULL, &timer);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_event_loop_create(&loop);
    assert(err == PPDB_OK);

    err = ppdb_base_timer_create(loop, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_timer_create(loop, &timer);
    assert(err == PPDB_OK);

    // Test invalid timer operations
    err = ppdb_base_timer_start(NULL, 100, false, test_timer_callback, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_timer_start(timer, 0, false, test_timer_callback, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_timer_start(timer, 100, false, NULL, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    // Cleanup
    ppdb_base_timer_destroy(timer);
    ppdb_base_event_loop_destroy(loop);
}

int main(void) {
    printf("Testing timer basic operations...\n");
    test_timer_basic();
    printf("PASSED\n");

    printf("Testing timer operations...\n");
    test_timer_operations();
    printf("PASSED\n");

    printf("Testing timer error handling...\n");
    test_timer_errors();
    printf("PASSED\n");

    return 0;
}