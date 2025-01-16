#include "plugin.h"

/* 导出函数 */
__attribute__((used))
int _dl_main(const host_api_t* api) {
    /* 使用主程序提供的接口 */
    char* msg = api->malloc(100);
    api->memset(msg, 0, 100);
    api->memcpy(msg, "Hello from plugin!\n", 18);
    api->printf(msg);
    api->printf("The answer is %d\n", 42);
    api->free(msg);
    return 42;
}

/* 主函数 - 用于独立运行 */
int main(void) {
    return _dl_main(0);
} 