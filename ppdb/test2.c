#include "cosmopolitan.h"

int main(int argc, char** argv) {
    printf("Hello from test2.c!\n");
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }
    return 0;
} 
