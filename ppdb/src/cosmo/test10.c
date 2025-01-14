/* 测试动态库 */
#include "cosmopolitan.h"

/* 全局变量示例 */
static int g_counter = 0;

/* 生命周期管理接口 */
__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
int dl_init(void) {
    g_counter = 42;
    dprintf(1, "dl_init: counter initialized to %d\n", g_counter);
    return 0;
}

__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
int dl_main(void) {
    g_counter++;
    dprintf(1, "dl_main: counter increased to %d\n", g_counter);
    return g_counter;
}

__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
int dl_fini(void) {
    int old_value = g_counter;
    dprintf(1, "dl_fini: final counter value is %d\n", g_counter);
    g_counter = 0;
    return old_value;
}

/* 自定义函数 */
__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
int get_counter(void) {
    return g_counter;
} 