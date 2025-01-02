#include "cosmopolitan.h"

static int test_return_42(void) {
    int value = 42;
    printf("Testing return 42...\n");
    printf("Value to return: %d\n", value);
    fflush(stdout);
    return value;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    printf("Running 42 test...\n");
    fflush(stdout);
    
    int result = test_return_42();
    printf("Test completed with result: %d\n", result);
    printf("Test %s\n", result == 42 ? "PASSED" : "FAILED");
    fflush(stdout);
    return result == 42 ? 0 : 1;
} 
