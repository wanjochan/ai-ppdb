#include <cosmopolitan.h>
#include "internal/base.h"

// Test data
static int event_count = 0;

// Event callback
static void test_event_callback(ppdb_base_event_t* event, void* data) {
    event_count++;
}

// Test event loop basic operations
static void test_event_loop_basic(void) {
    ppdb_error_t err;
    ppdb_base_event_loop_t* loop = NULL;
    ppdb_base_event_stats_t stats;

    // Create event loop
    err = ppdb_base_event_loop_create(&loop);
    assert(err == PPDB_OK);
    assert(loop != NULL);

    // Check initial stats
    ppdb_base_event_loop_get_stats(loop, &stats);
    assert(stats.total_events == 0);
    assert(stats.active_events == 0);
    assert(stats.total_dispatches == 0);

    // Cleanup
    ppdb_base_event_loop_destroy(loop);
}

// Test event registration and dispatch
static void test_event_registration(void) {
    ppdb_error_t err;
    ppdb_base_event_loop_t* loop = NULL;
    ppdb_base_event_t* event = NULL;
    ppdb_base_event_stats_t stats;

    // Create event loop
    err = ppdb_base_event_loop_create(&loop);
    assert(err == PPDB_OK);

    // Register event
    err = ppdb_base_event_create(loop, &event);
    assert(err == PPDB_OK);
    assert(event != NULL);

    // Set event callback
    err = ppdb_base_event_set_callback(event, test_event_callback, NULL);
    assert(err == PPDB_OK);

    // Check stats after registration
    ppdb_base_event_loop_get_stats(loop, &stats);
    assert(stats.active_events == 1);

    // Trigger event
    event_count = 0;
    err = ppdb_base_event_trigger(event);
    assert(err == PPDB_OK);

    // Run event loop
    err = ppdb_base_event_loop_run(loop, 100);
    assert(err == PPDB_OK);

    // Check results
    assert(event_count == 1);
    ppdb_base_event_loop_get_stats(loop, &stats);
    assert(stats.total_dispatches == 1);

    // Cleanup
    ppdb_base_event_destroy(event);
    ppdb_base_event_loop_destroy(loop);
}

// Test multiple events
static void test_multiple_events(void) {
    ppdb_error_t err;
    ppdb_base_event_loop_t* loop = NULL;
    ppdb_base_event_t* events[3] = {NULL};
    ppdb_base_event_stats_t stats;

    // Create event loop
    err = ppdb_base_event_loop_create(&loop);
    assert(err == PPDB_OK);

    // Register multiple events
    for (int i = 0; i < 3; i++) {
        err = ppdb_base_event_create(loop, &events[i]);
        assert(err == PPDB_OK);
        err = ppdb_base_event_set_callback(events[i], test_event_callback, NULL);
        assert(err == PPDB_OK);
    }

    // Check stats
    ppdb_base_event_loop_get_stats(loop, &stats);
    assert(stats.active_events == 3);

    // Trigger all events
    event_count = 0;
    for (int i = 0; i < 3; i++) {
        err = ppdb_base_event_trigger(events[i]);
        assert(err == PPDB_OK);
    }

    // Run event loop
    err = ppdb_base_event_loop_run(loop, 100);
    assert(err == PPDB_OK);

    // Check results
    assert(event_count == 3);
    ppdb_base_event_loop_get_stats(loop, &stats);
    assert(stats.total_dispatches == 3);

    // Cleanup
    for (int i = 0; i < 3; i++) {
        ppdb_base_event_destroy(events[i]);
    }
    ppdb_base_event_loop_destroy(loop);
}

// Test error handling
static void test_event_errors(void) {
    ppdb_error_t err;
    ppdb_base_event_loop_t* loop = NULL;
    ppdb_base_event_t* event = NULL;

    // Test invalid parameters
    err = ppdb_base_event_loop_create(NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_event_loop_create(&loop);
    assert(err == PPDB_OK);

    err = ppdb_base_event_create(NULL, &event);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_event_create(loop, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    // Test with valid event
    err = ppdb_base_event_create(loop, &event);
    assert(err == PPDB_OK);

    // Test invalid callback
    err = ppdb_base_event_set_callback(event, NULL, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    // Cleanup
    ppdb_base_event_destroy(event);
    ppdb_base_event_loop_destroy(loop);
}

int main(void) {
    printf("Testing event loop basic operations...\n");
    test_event_loop_basic();
    printf("PASSED\n");

    printf("Testing event registration and dispatch...\n");
    test_event_registration();
    printf("PASSED\n");

    printf("Testing multiple events...\n");
    test_multiple_events();
    printf("PASSED\n");

    printf("Testing event error handling...\n");
    test_event_errors();
    printf("PASSED\n");

    return 0;
} 