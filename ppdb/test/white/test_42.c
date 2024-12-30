#include "cosmopolitan.h"

static int test_return_42(void) {
    printf("Testing return 42...\n");
    return 0;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    printf("Running 42 test...\n");
    int result = test_return_42();
    printf("Test completed with result: %d\n", result);
    return result;
} 
