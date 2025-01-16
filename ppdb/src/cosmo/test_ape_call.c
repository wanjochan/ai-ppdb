#include "cosmopolitan.h"
#include "plugin.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <plugin>\n", argv[0]);
        return 1;
    }

    // 加载插件
    plugin_t* p = load_plugin(argv[1]);
    if (!p) {
        printf("Failed to load plugin\n");
        return 1;
    }

    // 执行插件主函数
    printf("Executing plugin main function...\n");
    printf("Main function pointer: %p\n", p->main);
    printf("Base address: %p\n", p->base);
    printf("Size: %ld\n", p->size);
    
    int ret = p->main();
    printf("Plugin main returned: %d\n", ret);

    // 卸载插件
    unload_plugin(p);

    return 0;
} 