#include "internal/polyx/PolyxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

// Test file operations
void test_polyx_async_read_file(void) {
    const char* test_file = "test.txt";
    
    // Create a test file
    FILE* fp = fopen(test_file, "w");
    assert(fp != NULL);
    fprintf(fp, "Hello, World!");
    fclose(fp);
    
    // Test async read file
    PolyxAsync* async = PolyxAsyncClass.read_file(test_file);
    assert(async != NULL);
    
    async = async->start(async);
    assert(async != NULL);
    
    // Wait for completion
    while (!async->is_done(async)) {
        // Simulate async loop
        usleep(10000);  // 10ms
    }
    
    // Check result
    PolyxAsyncResult* result = async->get_result(async);
    assert(result != NULL);
    assert(strcmp("Hello, World!", result->data) == 0);
    
    // Cleanup
    PolyxAsyncClass.free(async);
    remove(test_file);
}

void test_polyx_async_write_file(void) {
    const char* test_file = "test_write.txt";
    const char* test_data = "Hello, Write Test!";
    
    // Test async write file
    PolyxAsync* async = PolyxAsyncClass.write_file(test_file, test_data, strlen(test_data));
    assert(async != NULL);
    
    async = async->start(async);
    assert(async != NULL);
    
    // Wait for completion
    while (!async->is_done(async)) {
        // Simulate async loop
        usleep(10000);  // 10ms
    }
    
    // Verify file contents
    FILE* fp = fopen(test_file, "r");
    assert(fp != NULL);
    
    char buffer[100];
    size_t read = fread(buffer, 1, sizeof(buffer), fp);
    buffer[read] = '\0';
    fclose(fp);
    
    assert(strcmp(test_data, buffer) == 0);
    
    // Cleanup
    PolyxAsyncClass.free(async);
    remove(test_file);
}

void test_polyx_async_delay(void) {
    int delay_ms = 100;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Test async delay
    PolyxAsync* async = PolyxAsyncClass.delay(delay_ms);
    assert(async != NULL);
    
    async = async->start(async);
    assert(async != NULL);
    
    // Wait for completion
    while (!async->is_done(async)) {
        // Simulate async loop
        usleep(10000);  // 10ms
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate elapsed time in milliseconds
    long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 +
                     (end.tv_nsec - start.tv_nsec) / 1000000;
    
    // Allow for some timing variance
    assert(elapsed_ms >= delay_ms);
    assert(elapsed_ms < delay_ms + 50);  // Allow 50ms variance
    
    // Cleanup
    PolyxAsyncClass.free(async);
}

void test_polyx_async_parallel(void) {
    const char* test_file1 = "test1.txt";
    const char* test_file2 = "test2.txt";
    
    // Create test files
    FILE* fp = fopen(test_file1, "w");
    assert(fp != NULL);
    fprintf(fp, "Test File 1");
    fclose(fp);
    
    fp = fopen(test_file2, "w");
    assert(fp != NULL);
    fprintf(fp, "Test File 2");
    fclose(fp);
    
    // Create async tasks
    PolyxAsync* tasks[2];
    tasks[0] = PolyxAsyncClass.read_file(test_file1);
    tasks[1] = PolyxAsyncClass.read_file(test_file2);
    
    assert(tasks[0] != NULL);
    assert(tasks[1] != NULL);
    
    // Run tasks in parallel
    PolyxAsync* parallel = PolyxAsyncClass.parallel(tasks, 2);
    assert(parallel != NULL);
    
    parallel = parallel->start(parallel);
    assert(parallel != NULL);
    
    // Wait for completion
    while (!parallel->is_done(parallel)) {
        usleep(10000);  // 10ms
    }
    
    // Cleanup
    PolyxAsyncClass.free(parallel);
    PolyxAsyncClass.free(tasks[0]);
    PolyxAsyncClass.free(tasks[1]);
    remove(test_file1);
    remove(test_file2);
}

void test_polyx_async_sequence(void) {
    const char* test_file = "test_seq.txt";
    const char* test_data = "Test Sequence";
    
    // Create async tasks
    PolyxAsync* tasks[2];
    tasks[0] = PolyxAsyncClass.write_file(test_file, test_data, strlen(test_data));
    tasks[1] = PolyxAsyncClass.read_file(test_file);
    
    assert(tasks[0] != NULL);
    assert(tasks[1] != NULL);
    
    // Run tasks in sequence
    PolyxAsync* sequence = PolyxAsyncClass.sequence(tasks, 2);
    assert(sequence != NULL);
    
    sequence = sequence->start(sequence);
    assert(sequence != NULL);
    
    // Wait for completion
    while (!sequence->is_done(sequence)) {
        usleep(10000);  // 10ms
    }
    
    // Verify results
    PolyxAsyncResult* result = tasks[1]->get_result(tasks[1]);
    assert(result != NULL);
    assert(strcmp(test_data, result->data) == 0);
    
    // Cleanup
    PolyxAsyncClass.free(sequence);
    PolyxAsyncClass.free(tasks[0]);
    PolyxAsyncClass.free(tasks[1]);
    remove(test_file);
}

int main(void) {
    printf("Running PolyxAsync tests...\n");
    
    test_polyx_async_read_file();
    test_polyx_async_write_file();
    test_polyx_async_delay();
    test_polyx_async_parallel();
    test_polyx_async_sequence();
    
    printf("All tests passed!\n");
    return 0;
}
