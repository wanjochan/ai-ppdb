#include "cosmopolitan.h"
#include "ape/ape.h"
#include "libc/elf/elf.h"
#include "libc/runtime/runtime.h"

int main(int argc, char* argv[]) {
    long sp[2] = {0, 0};
    printf("before call ApeLoader()");
    return ApeLoader(argc, sp, 0);
} 
