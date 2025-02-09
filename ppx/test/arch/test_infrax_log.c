#include <stdio.h>
#include "internal/infrax/InfraxLog.h"
#include "internal/infrax/InfraxCore.h"
// #include <assert.h> use assert from our core

void test_log_basic() {
    printf("Testing basic logging...\n");
    
    // Get core instance
    InfraxCore* core = InfraxCoreClass.singleton();
    
    // Test different log levels
    InfraxLog* log = InfraxLogClass.new();
    INFRAX_ASSERT(core, log != NULL);
    
    log->debug(log, "Debug message");
    log->info(log, "Info message");
    log->warn(log, "Warning message");
    log->error(log, "Error message");
    
    InfraxLogClass.free(log);
    
    printf("Basic logging test passed\n");
}

void test_log_format() {
    printf("Testing log formatting...\n");
    
    // Get core instance
    InfraxCore* core = InfraxCoreClass.singleton();
    
    InfraxLog* log = InfraxLogClass.new();
    INFRAX_ASSERT(core, log != NULL);
    
    // Test formatted messages
    log->debug(log, "Debug: %d", 42);
    log->info(log, "Info: %s", "Hello");
    log->warn(log, "Warning: %f", 3.14);
    log->error(log, "Error: %x", 0xFF);
    
    InfraxLogClass.free(log);
    
    printf("Log formatting test passed\n");
}

int main() {
    printf("===================\nStarting InfraxLog tests...\n");
    
    test_log_basic();
    test_log_format();
    
    printf("All InfraxLog tests passed!\n===================\n");
    return 0;
}
