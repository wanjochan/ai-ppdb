#include <cosmopolitan.h>
#include "internal/base.h"

// Test data
static int event_count = 0;

// Event callback
static void test_event_callback(ppdb_base_event_t* event, void* data) {
    event_count++;
}

// Test event basic operations
static void test_event_basic(void) {
    ppdb_error_t err;
    ppdb_base_event_loop_t* loop = NULL;
    ppdb_base_event_t* event = NULL;
    ppdb_base_event_stats_t stats;

    // Create event loop
    err = ppdb_base_event_loop_create(&loop);
    assert(err == PPDB_OK);
    assert(loop != NULL);

    // Create event
    err = ppdb_base_event_create(loop, &event);
    assert(err == PPDB_OK);
    assert(event != NULL);

    // Check initial stats
    ppdb_base_event_get_stats(event, &stats);
    assert(stats.total_events == 0);
    assert(stats.active_events == 0);
    assert(stats.total_filters == 0);

    // Cleanup
    ppdb_base_event_destroy(event);
    ppdb_base_event_loop_destroy(loop);
}

// Test event filtering
static void test_event_filtering(void) {
    ppdb_error_t err;
    ppdb_base_event_loop_t* loop = NULL;
    ppdb_base_event_t* event = NULL;
    ppdb_base_event_filter_t filter;
    ppdb_base_event_stats_t stats;

    // Setup
    err = ppdb_base_event_loop_create(&loop);
    assert(err == PPDB_OK);
    err = ppdb_base_event_create(loop, &event);
    assert(err == PPDB_OK);

    // Add IO filter
    filter.type = PPDB_EVENT_TYPE_IO;
    filter.priority = PPDB_EVENT_PRIORITY_HIGH;
    err = ppdb_base_event_add_filter(event, &filter);
    assert(err == PPDB_OK);

    // Add timer filter
    filter.type = PPDB_EVENT_TYPE_TIMER;
    filter.priority = PPDB_EVENT_PRIORITY_NORMAL;
    err = ppdb_base_event_add_filter(event, &filter);
    assert(err == PPDB_OK);

    // Check stats
    ppdb_base_event_get_stats(event, &stats);
    assert(stats.total_filters == 2);

    // Remove filter
    err = ppdb_base_event_remove_filter(event, PPDB_EVENT_TYPE_TIMER);
    assert(err == PPDB_OK);

    ppdb_base_event_get_stats(event, &stats);
    assert(stats.total_filters == 1);

    // Cleanup
    ppdb_base_event_destroy(event);
    ppdb_base_event_loop_destroy(loop);
}

// Test cross-platform event handling
static void test_event_cross_platform(void) {
    ppdb_error_t err;
    ppdb_base_event_loop_t* loop = NULL;
    ppdb_base_event_t* event = NULL;
    ppdb_base_event_filter_t filter;
    int pipe_fds[2];

    // Create event loop
    err = ppdb_base_event_loop_create(&loop);
    assert(err == PPDB_OK);

    // Create event
    err = ppdb_base_event_create(loop, &event);
    assert(err == PPDB_OK);

    // Create pipe (works on all platforms via Cosmopolitan)
    assert(pipe(pipe_fds) == 0);

    // Setup IO event filter
    filter.type = PPDB_EVENT_TYPE_IO;
    filter.priority = PPDB_EVENT_PRIORITY_HIGH;
    err = ppdb_base_event_add_filter(event, &filter);
    assert(err == PPDB_OK);

    // Register read event
    event_count = 0;
    err = ppdb_base_event_register_io(event, pipe_fds[0], PPDB_EVENT_READ, test_event_callback, NULL);
    assert(err == PPDB_OK);

    // Write to pipe to trigger event
    assert(write(pipe_fds[1], "test", 4) == 4);

    // Run event loop
    err = ppdb_base_event_loop_run(loop, 100);
    assert(err == PPDB_OK);
    assert(event_count == 1);

    // Cleanup
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    ppdb_base_event_destroy(event);
    ppdb_base_event_loop_destroy(loop);
}

// Test event error handling
static void test_event_errors(void) {
    ppdb_error_t err;
    ppdb_base_event_loop_t* loop = NULL;
    ppdb_base_event_t* event = NULL;
    ppdb_base_event_filter_t filter;

    // Test invalid parameters
    err = ppdb_base_event_create(NULL, &event);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_event_loop_create(&loop);
    assert(err == PPDB_OK);

    err = ppdb_base_event_create(loop, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_event_create(loop, &event);
    assert(err == PPDB_OK);

    // Test invalid filter operations
    err = ppdb_base_event_add_filter(NULL, &filter);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_event_add_filter(event, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    // Test removing non-existent filter
    err = ppdb_base_event_remove_filter(event, PPDB_EVENT_TYPE_SIGNAL);
    assert(err == PPDB_BASE_ERR_NOT_FOUND);

    // Cleanup
    ppdb_base_event_destroy(event);
    ppdb_base_event_loop_destroy(loop);
}

int main(void) {
    printf("Testing event basic operations...\n");
    test_event_basic();
    printf("PASSED\n");

    printf("Testing event filtering...\n");
    test_event_filtering();
    printf("PASSED\n");

    printf("Testing cross-platform event handling...\n");
    test_event_cross_platform();
    printf("PASSED\n");

    printf("Testing event error handling...\n");
    test_event_errors();
    printf("PASSED\n");

    return 0;
} 
} 