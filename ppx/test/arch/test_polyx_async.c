#include "internal/polyx/PolyxAsync.h"
#include <stdio.h>
#include <assert.h>

// Test file operations
void test_file_operations(void) {
    printf("Testing file operations...\n");
    
    // Test file read
    PolyxAsync* read_task = PolyxAsync_CLASS.read_file("test.txt");
    assert(read_task != NULL);
    read_task->start(read_task);
    assert(read_task->is_done(read_task));
    PolyxAsync_CLASS.free(read_task);
    
    printf("File operations test passed\n");
}

// Test timer operations
void test_timer_operations(void) {
    printf("Testing timer operations...\n");
    
    // Test delay
    PolyxAsync* delay_task = PolyxAsync_CLASS.delay(100);  // 100ms
    assert(delay_task != NULL);
    delay_task->start(delay_task);
    assert(delay_task->is_done(delay_task));
    PolyxAsync_CLASS.free(delay_task);
    
    printf("Timer operations test passed\n");
}

// Test concurrent operations
void test_concurrent_operations(void) {
    printf("Testing concurrent operations...\n");
    
    // Create multiple tasks
    PolyxAsync* tasks[2];
    tasks[0] = PolyxAsync_CLASS.delay(50);   // 50ms
    tasks[1] = PolyxAsync_CLASS.delay(100);  // 100ms
    
    // Test parallel execution
    PolyxAsync* parallel_task = PolyxAsync_CLASS.parallel(tasks, 2);
    assert(parallel_task != NULL);
    parallel_task->start(parallel_task);
    assert(parallel_task->is_done(parallel_task));
    
    // Cleanup
    PolyxAsync_CLASS.free(parallel_task);
    
    printf("Concurrent operations test passed\n");
}

int main(void) {
    printf("Starting PolyxAsync tests...\n");
    
    test_file_operations();
    test_timer_operations();
    test_concurrent_operations();
    
    printf("All tests passed!\n");
    return 0;
}
