#include "cosmopolitan.h"

// DLL 入口点
int module_main(void) {
    #if defined(_WIN32)
    // Windows specific initialization
    return 1;  // TRUE for Windows DLL_PROCESS_ATTACH
    #else
    // Unix-like systems initialization
    return 0;  // Success
    #endif
}

// 导出函数
__attribute__((visibility("default")))
int test4_func(void) {
    return 42;
} 