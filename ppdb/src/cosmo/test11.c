#include "cosmopolitan.h"

/* 导出函数 */
__attribute__((visibility("default")))
int dl_main(void) {
    dprintf(1, "Hello from test11!\n");
    return 0;
}

/* 初始化函数 */
__attribute__((visibility("default")))
int dl_init(void) {
    dprintf(1, "Initializing test11...\n");
    return 0;
}

/* 清理函数 */
__attribute__((visibility("default")))
int dl_fini(void) {
    dprintf(1, "Cleaning up test11...\n");
    return 0;
}

/* 测试函数 */
__attribute__((visibility("default")))
int test_func(int x, int y) {
    return x + y;
} 