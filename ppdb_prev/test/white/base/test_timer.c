#include <cosmopolitan.h>
#include "internal/base.h"

// Test data
static _Atomic(int) timer_count = 0;
static _Atomic(uint64_t) total_drift = 0;
static const int NUM_TIMERS = 1000;
static const int NUM_THREADS = 4;

// Timer callback
static void test_timer_callback(ppdb_base_timer_t* timer, void* data) {
    atomic_fetch_add(&timer_count, 1);
    
    uint64_t now;
    ppdb_base_time_get_microseconds(&now);
    uint64_t actual_elapsed = (now - timer->next_timeout) / 1000;
    int64_t drift = actual_elapsed - timer->interval_ms;
    atomic_fetch_add(&total_drift, (drift > 0) ? drift : -drift);
}

// Test timer basic operations
static int test_timer_basic(void) {
    ppdb_base_timer_t* timer = NULL;
    uint64_t stats_total, stats_min, stats_max, stats_avg, stats_last, stats_drift;
    
    // Create timer
    ASSERT_OK(ppdb_base_timer_create(&timer, 100));
    ASSERT_NOT_NULL(timer);
    
    // Set callback
    timer->callback = test_timer_callback;
    timer->repeating = false;
    
    // Reset counters
    atomic_store(&timer_count, 0);
    atomic_store(&total_drift, 0);
    
    // Run timer
    for (int i = 0; i < 10; i++) {
        ASSERT_OK(ppdb_base_timer_update());
        ppdb_base_sleep(10);
    }
    
    // Check results
    ASSERT_OK(ppdb_base_timer_get_stats(timer, &stats_total, &stats_min,
                                      &stats_max, &stats_avg, &stats_last,
                                      &stats_drift));
    ASSERT_EQ(atomic_load(&timer_count), 1);
    
    // Cleanup
    ASSERT_OK(ppdb_base_timer_destroy(timer));
    return 0;
}

// Test timer wheel operations
static int test_timer_wheel(void) {
    ppdb_base_timer_t* timers[4];
    uint64_t intervals[4] = {10, 100, 1000, 10000}; // Test different wheels
    
    // Create timers
    for (int i = 0; i < 4; i++) {
        ASSERT_OK(ppdb_base_timer_create(&timers[i], intervals[i]));
        timers[i]->callback = test_timer_callback;
        timers[i]->repeating = true;
    }
    
    // Reset counters
    atomic_store(&timer_count, 0);
    atomic_store(&total_drift, 0);
    
    // Run timers
    for (int i = 0; i < 100; i++) {
        ASSERT_OK(ppdb_base_timer_update());
        ppdb_base_sleep(10);
    }
    
    // Check results
    uint64_t total_timers, active_timers, expired_timers, overdue_timers, total_drift;
    ppdb_base_timer_get_manager_stats(&total_timers, &active_timers,
                                   &expired_timers, &overdue_timers,
                                   &total_drift);
    ASSERT_EQ(total_timers, 4);
    ASSERT_EQ(active_timers, 4);
    
    // Cleanup
    for (int i = 0; i < 4; i++) {
        ASSERT_OK(ppdb_base_timer_destroy(timers[i]));
    }
    return 0;
}

// Thread function for concurrent test
static void timer_thread_func(void* arg) {
    int thread_id = *(int*)arg;
    ppdb_base_timer_t* timers[NUM_TIMERS / NUM_THREADS];
    
    // Create timers
    for (int i = 0; i < NUM_TIMERS / NUM_THREADS; i++) {
        uint64_t interval = 10 + (rand() % 1000); // Random interval 10-1010ms
        ASSERT_OK(ppdb_base_timer_create(&timers[i], interval));
        timers[i]->callback = test_timer_callback;
        timers[i]->repeating = (rand() % 2) == 0; // Random repeating
    }
    
    // Run updates
    for (int i = 0; i < 100; i++) {
        ASSERT_OK(ppdb_base_timer_update());
        ppdb_base_sleep(1);
    }
    
    // Cleanup
    for (int i = 0; i < NUM_TIMERS / NUM_THREADS; i++) {
        ASSERT_OK(ppdb_base_timer_destroy(timers[i]));
    }
}

// Test concurrent timer operations
static int test_timer_concurrent(void) {
    ppdb_base_thread_t* threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    // Reset counters
    atomic_store(&timer_count, 0);
    atomic_store(&total_drift, 0);
    
    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        ASSERT_OK(ppdb_base_thread_create(&threads[i], timer_thread_func, &thread_ids[i]));
    }
    
    // Wait for threads
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i]));
        ASSERT_OK(ppdb_base_thread_destroy(threads[i]));
    }
    
    // Check results
    uint64_t total_timers, active_timers, expired_timers, overdue_timers, total_drift;
    ppdb_base_timer_get_manager_stats(&total_timers, &active_timers,
                                   &expired_timers, &overdue_timers,
                                   &total_drift);
    ASSERT_TRUE(atomic_load(&timer_count) > 0);
    ASSERT_EQ(active_timers, 0);
    
    return 0;
}

// Test timer error handling
static int test_timer_errors(void) {
    ppdb_base_timer_t* timer = NULL;
    
    // Test invalid parameters
    ASSERT_ERROR(ppdb_base_timer_create(NULL, 100));
    ASSERT_ERROR(ppdb_base_timer_create(&timer, 0));
    
    // Create valid timer
    ASSERT_OK(ppdb_base_timer_create(&timer, 100));
    
    // Test invalid operations
    ASSERT_ERROR(ppdb_base_timer_destroy(NULL));
    ASSERT_ERROR(ppdb_base_timer_get_stats(NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    
    // Cleanup
    ASSERT_OK(ppdb_base_timer_destroy(timer));
    return 0;
}

// Performance test
static int test_timer_performance(void) {
    ppdb_base_timer_t* timers[NUM_TIMERS];
    uint64_t start_time, end_time;
    
    // Reset counters
    atomic_store(&timer_count, 0);
    atomic_store(&total_drift, 0);
    
    // Create timers
    ASSERT_OK(ppdb_base_time_get_microseconds(&start_time));
    for (int i = 0; i < NUM_TIMERS; i++) {
        uint64_t interval = 10 + (rand() % 1000);
        ASSERT_OK(ppdb_base_timer_create(&timers[i], interval));
        timers[i]->callback = test_timer_callback;
        timers[i]->repeating = true;
    }
    ASSERT_OK(ppdb_base_time_get_microseconds(&end_time));
    printf("Timer creation time: %lu us/timer\n", (end_time - start_time) / NUM_TIMERS);
    
    // Run updates
    ASSERT_OK(ppdb_base_time_get_microseconds(&start_time));
    for (int i = 0; i < 1000; i++) {
        ASSERT_OK(ppdb_base_timer_update());
        ppdb_base_sleep(1);
    }
    ASSERT_OK(ppdb_base_time_get_microseconds(&end_time));
    printf("Timer update time: %lu us/update\n", (end_time - start_time) / 1000);
    
    // Check drift
    printf("Average timer drift: %lu us\n", atomic_load(&total_drift) / atomic_load(&timer_count));
    
    // Cleanup
    for (int i = 0; i < NUM_TIMERS; i++) {
        ASSERT_OK(ppdb_base_timer_destroy(timers[i]));
    }
    return 0;
}

int main(void) {
    printf("Testing timer basic operations...\n");
    TEST_RUN(test_timer_basic);
    printf("PASSED\n");
    
    printf("Testing timer wheel operations...\n");
    TEST_RUN(test_timer_wheel);
    printf("PASSED\n");
    
    printf("Testing concurrent timer operations...\n");
    TEST_RUN(test_timer_concurrent);
    printf("PASSED\n");
    
    printf("Testing timer error handling...\n");
    TEST_RUN(test_timer_errors);
    printf("PASSED\n");
    
    printf("Testing timer performance...\n");
    TEST_RUN(test_timer_performance);
    printf("PASSED\n");
    
    return 0;
}