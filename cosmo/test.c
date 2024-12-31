static const char message[] = "Hello from module_main!\n";
static const char message1[] = "Hello from test_func1!\n";
static const char message2[] = "Hello from test_func2!\n";

__attribute__((section(".text.module_main")))
int module_main(void) {
    return 42;
}

__attribute__((section(".text.test_func1")))
int test_func1(int x) {
    return x * 2;
}

__attribute__((section(".text.test_func2")))
int test_func2(int x, int y) {
    return x + y;
} 