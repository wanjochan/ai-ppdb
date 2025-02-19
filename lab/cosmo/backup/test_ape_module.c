#include "plugin.h"
#include "ape_module.h"

/* 主程序提供的接口实现 */
static host_api_t host_api = {
    .printf = printf,
    .malloc = malloc,
    .free = free,
    .memcpy = memcpy,
    .memset = memset
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <module>\n", argv[0]);
        return 1;
    }

    /* 加载模块 */
    plugin_t* p = load_plugin(argv[1]);
    if (!p) {
        printf("Failed to load module\n");
        return 1;
    }

    /* 查找并调用主函数 */
    if (p->main) {
        int ret = p->main(&host_api);
        printf("Module returned %d\n", ret);
    }

    /* 卸载模块 */
    unload_plugin(p);
    return 0;
} 