#include "cosmopolitan.h"

// DLL 入口点
__attribute__((section(".text.startup")))
int module_main(void) {
    return 0;
}

// 导出函数
int test4_func(void) {
    return 42;
} 