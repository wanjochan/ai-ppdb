#include "cosmopolitan.h"

int main(int argc, char* argv[]) {
    printf("Test module loaded.\n");
    printf("Arguments received: %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("arg[%d]: %s\n", i, argv[i]);
    }
    return 0;
}
