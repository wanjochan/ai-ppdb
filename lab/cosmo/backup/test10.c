/* 测试动态库 */
#include "cosmopolitan.h"

/* 全局变量示例 */
static int g_counter = 0;
static char g_buffer[256];

/* 生命周期管理接口 */
__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
int dl_init(void) {
    /* 测试字符串操作函数 */
    strcpy(g_buffer, "Hello from dl_init");
    g_counter = strlen(g_buffer);
    dprintf(1, "dl_init: buffer='%s', counter=%d\n", g_buffer, g_counter);
    return 0;
}

__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
int dl_main(void) {
    /* 测试格式化输出和数学函数 */
    g_counter++;
    snprintf(g_buffer, sizeof(g_buffer), "Counter: %d, Square: %d", 
             g_counter, g_counter * g_counter);
    dprintf(1, "dl_main: %s\n", g_buffer);
    return g_counter;
}

__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
int dl_fini(void) {
    /* 测试内存操作函数 */
    int old_value = g_counter;
    memset(g_buffer, 0, sizeof(g_buffer));
    g_counter = 0;
    dprintf(1, "dl_fini: cleared buffer, final counter was %d\n", old_value);
    return old_value;
}

/* 自定义函数 */
__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
int get_counter(void) {
    return g_counter;
}

__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
const char* get_buffer(void) {
    return g_buffer;
} 