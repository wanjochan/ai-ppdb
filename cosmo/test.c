#include "cosmopolitan.h"

__attribute__((section(".text.module_main")))
int module_main(void) {
    const char *msg = "Hello from module_main!\n";
    write(1, msg, strlen(msg));
    return 42;
}

__attribute__((section(".text.test_func1")))
int test_func1(int x) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "Hello from test_func1! x=%d\n", x);
    write(1, buf, len);
    return x * 2;
}

__attribute__((section(".text.test_func2")))
int test_func2(int x, int y) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "Hello from test_func2! x=%d, y=%d\n", x, y);
    write(1, buf, len);
    return x + y;
} 