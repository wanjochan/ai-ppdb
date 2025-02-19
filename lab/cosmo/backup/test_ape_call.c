#include "cosmopolitan.h"
#include "plugin.h"

/* 主程序导出的接口实现 */
static host_api_t host_api = {
    .printf = printf,
    .malloc = malloc,
    .free = free,
    .memcpy = memcpy,
    .memset = memset
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <plugin>\n", argv[0]);
        return 1;
    }

    plugin_t* p = load_plugin(argv[1]);
    if (!p) {
        printf("Failed to load plugin\n");
        return 1;
    }

    printf("Executing plugin main function...\n");
    printf("Main function pointer: %p\n", p->main);
    printf("Base address: %p\n", p->base);
    printf("Size: %ld\n", p->size);

    int ret = p->main(&host_api);
    printf("Plugin main returned: %d\n", ret);

    unload_plugin(p);
    return 0;
} 