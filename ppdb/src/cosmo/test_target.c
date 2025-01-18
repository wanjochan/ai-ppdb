#include "cosmopolitan.h"

int main(int argc, char *argv[]) {
    printf("Hello from test_target!\n");
    printf("Arguments: %d\n", argc);
    for(int i = 0; i < argc; i++) {
        printf("  argv[%d] = %s\n", i, argv[i]);
    }
    return 0;
}

//void _start(void) {
//    exit(main(__argc, __argv));
//} 
