#include "cosmopolitan.h"

int test_func(int a, int b) {
    return a + b;
}

int main(int argc, char* argv[]) {
    printf("Hello from test_target!\n");
    printf("1 + 2 = %d\n", test_func(1, 2));
    return 0;
}

void _start(void) {
    static char* argv[] = {"test_target.com", NULL};
    exit(main(1, argv));
} 
