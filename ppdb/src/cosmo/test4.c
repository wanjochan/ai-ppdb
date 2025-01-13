#include "cosmopolitan.h"

// DLL 入口点
__attribute__((visibility("default")))
int module_main(void) {
    return 0;
}

// 导出函数
__attribute__((visibility("default")))
int test4_func(void) {
    return 42;
} 