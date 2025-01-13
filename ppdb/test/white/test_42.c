#include "infra/infra_core.h"
#include "infra/infra_printf.h"

static int test_return_42(void) {
    int value = 42;
    infra_printf("Testing return 42...\n");
    infra_printf("Value to return: %d\n", value);
    return value;
}

static int test_main(void) {
    infra_printf("Running 42 test...\n");
    
    int result = test_return_42();
    infra_printf("Test completed with result: %d\n", result);
    infra_printf("Test %s\n", result == 42 ? "PASSED" : "FAILED");
    return result == 42 ? 0 : 1;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return test_main();
}
