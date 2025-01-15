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

/* 插件函数 */
__attribute__((section(".text.dl_init")))
__attribute__((used))
int dl_init(void) {
    write(1, "[Plugin] Init called\n", 20);
    return 0;
}

__attribute__((section(".text.dl_main")))
__attribute__((used))
int dl_main(void) {
    write(1, "[Plugin] Main called\n", 20);
    return 42;
}

__attribute__((section(".text.dl_fini")))
__attribute__((used))
int dl_fini(void) {
    write(1, "[Plugin] Fini called\n", 20);
    return 0;
} 