#include <stdio.h>

/* 生命周期管理接口 */
__attribute__((visibility("default")))
int dl_init(void) {
    return 0;
}

__attribute__((visibility("default")))
int dl_main(void) {
    return 0;
}

__attribute__((visibility("default")))
int dl_fini(void) {
    return 0;
}

/* 导出函数 */
__attribute__((visibility("default")))
int test_func(int x) {
    printf("Called test_func with: %d\n", x);
    return x * 2;
} 