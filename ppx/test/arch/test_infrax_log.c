#include "internal/infrax/InfraxLog.h"
#include "internal/infrax/InfraxCore.h"
// #include <assert.h> use assert from our core

InfraxCore* core = NULL;
void test_log_basic() {
    core->printf(core, "Testing basic logging...\n");
    
    // Test different log levels
    InfraxLog* log = InfraxLogClass.new();
    INFRAX_ASSERT(core, log != NULL);
    
    log->debug(log, "Debug message");
    log->info(log, "Info message");
    log->warn(log, "Warning message");
    log->error(log, "Error message");
    
    InfraxLogClass.free(log);
    
    core->printf(core, "Basic logging test passed\n");
}

void test_log_format() {
    core->printf(core, "Testing log formatting...\n");
    
    InfraxLog* log = InfraxLogClass.new();
    INFRAX_ASSERT(core, log != NULL);
    
    // Test formatted messages
    log->debug(log, "Debug: %d", 42);
    log->info(log, "Info: %s", "Hello");
    log->warn(log, "Warning: %f", 3.14);
    log->error(log, "Error: %x", 0xFF);
    
    InfraxLogClass.free(log);
    
    core->printf(core, "Log formatting test passed\n");
}

int main() {
    core = InfraxCoreClass.singleton();
    core->printf(core, "===================\nStarting InfraxLog tests...\n");
    
    test_log_basic();
    test_log_format();
    
    core->printf(core, "All InfraxLog tests passed!\n===================\n");
    return 0;
}
