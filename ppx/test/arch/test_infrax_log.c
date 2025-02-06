#include <stdio.h>
#include "internal/infrax/InfraxLog.h"
#include <assert.h>

int main() {
    // Create a new logger instance
    InfraxLog* logger = InfraxLog_CLASS.new();
    if (!logger) {
        fprintf(stderr, "Failed to create logger\n");
        return 1;
    }
    
    // Verify class pointer
    assert(logger->klass == &InfraxLog_CLASS);

    // Test different log levels
    logger->debug(logger, "This is a debug message");
    logger->info(logger, "This is an info message");
    logger->warn(logger, "This is a warning message");
    logger->error(logger, "This is an error message");

    // Test with format strings
    logger->info(logger, "Testing with number: %d", 42);
    logger->info(logger, "Testing with string: %s", "Hello World");

    // Test level filtering
    logger->set_level(logger, LOG_LEVEL_WARN);
    logger->debug(logger, "This debug message should not appear");
    logger->info(logger, "This info message should not appear");
    logger->warn(logger, "This warning message should appear");
    logger->error(logger, "This error message should appear");

    // Test global instance
    InfraxLog* global_logger = get_global_infrax_log();
    assert(global_logger != NULL);
    assert(global_logger->klass == &InfraxLog_CLASS);

    // Clean up
    InfraxLog_CLASS.free(logger);
    printf("All tests completed successfully\n");
    return 0;
}
