#include <stdint.h>

// Export a simple function
__attribute__((visibility("default")))
int add(int a, int b) {
    return a + b;
}

// Export a function that returns a string
__attribute__((visibility("default")))
const char* get_version(void) {
    return "APE-DL PoC v0.1";
}

// Library initialization function
__attribute__((constructor))
static void init(void) {
    // Optional initialization code
    volatile int dummy = 1;  // Just to make sure it's not optimized away
} 