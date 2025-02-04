#include <stdio.h>
#include "internal/infra/InfraLog.h"

int main() {
    // Create a new logger instance
    InfraLog* logger = infra_log_new();
    if (!logger) {
        fprintf(stderr, "Failed to create logger\n");
        return 1;
    }

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

    // Clean up
    logger->free(logger);
    printf("All tests completed successfully\n");
    return 0;
}
