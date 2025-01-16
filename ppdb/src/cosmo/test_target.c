#include "cosmopolitan.h"

/* 包装函数 */
void* __wrap_ape_stack_round(void* p) {
    return p;
}

/* 入口函数 */
int main(void) {
    write(1, "Hello from target APE!\n", 22);
    return 42;
} 