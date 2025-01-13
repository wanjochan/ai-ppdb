#include "cosmopolitan.h"

// 导出函数声明
__attribute__((visibility("default")))
int test4_add(int a, int b) {
    return a + b;
}

__attribute__((visibility("default")))
const char* test4_version(void) {
    return "test4 v1.0.0";
}

// 动态库入口点
__attribute__((visibility("default")))
int module_main(void) {
    write(1, "test4.dl loaded successfully!\n", 29);
    return 0;
} 