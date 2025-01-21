/* 简单的插件示例 */
#include "cosmopolitan.h"

/* 包装函数 */
int __wrap_main(void) {
    return 0;
}

void __wrap__init(void) {
}

void* __wrap_ape_stack_round(void* p) {
    return p;
}

int __wrap___cxa_atexit(void (*func)(void*), void* arg, void* dso) {
    return 0;
}

/* 插件函数 */
__attribute__((section(".text.dl_init")))
__attribute__((used))
int dl_init(void) {
    const char msg[] = "[Plugin] Init called\n";
    write(1, msg, sizeof(msg) - 1);
    return 0;
}

__attribute__((section(".text.dl_main")))
__attribute__((used))
int dl_main(void) {
    const char msg[] = "[Plugin] Main called\n";
    write(1, msg, sizeof(msg) - 1);
    return 42;
}

__attribute__((section(".text.dl_fini")))
__attribute__((used))
int dl_fini(void) {
    const char msg[] = "[Plugin] Fini called\n";
    write(1, msg, sizeof(msg) - 1);
    return 0;
} 