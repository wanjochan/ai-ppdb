#include "cosmopolitan.h"

//__attribute__((visibility("default")))
//__attribute__((section(".text.hot.test_func")))
int test_func(int x, int y) {
    //printf("Test function called with: x=%d, y=%d\n", x, y);
    //return x + y;
    return 42;
}

int main(int argc, char *argv[]) {
    //printf("Target program loaded!\n");
    return 0;
}

/* APE入口点 */
__attribute__((force_align_arg_pointer))
void _start(void) {
    static char* argv[] = {"test_target.com", NULL};
    exit(main(1, argv));
} 
