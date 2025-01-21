#include "cosmopolitan.h"

__attribute__((visibility("default")))
__attribute__((used))
int module_main(int argc, char* argv[]) {
    printf("Hello from main.dat!\n");
    printf("Arguments:\n");
    for (int i = 0; i < argc; i++) {
        printf("  %d: %s\n", i, argv[i]);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    return module_main(argc, argv);
}
