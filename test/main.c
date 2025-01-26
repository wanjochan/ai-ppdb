#include "libc/dlopen/dlfcn.h"
#include <stdio.h>

/* 插件头部魔数和版本 */
#define PLUGIN_MAGIC 0x50504442
#define PLUGIN_VERSION 1

/* 插件头部结构 */
struct plugin_header {
    uint32_t magic;
    uint32_t version;
    uint32_t init_offset;
    uint32_t main_offset;
    uint32_t fini_offset;
};

/* 加载插件 */
static void* load_plugin(const char* path, size_t* size) {
    printf("Loading plugin: %s\n", path);
    
    /* 加载动态库 */
    void* handle = cosmo_dlopen(path, RTLD_NOW);
    if (!handle) {
        printf("Failed to load plugin: %s\n", cosmo_dlerror());
        return NULL;
    }
    printf("Plugin loaded successfully\n");

    /* 获取函数指针 */
    int (*func)(int) = cosmo_dlsym(handle, "test_func");
    if (!func) {
        printf("Failed to find function: %s\n", cosmo_dlerror());
        cosmo_dlclose(handle);
        return NULL;
    }

    return func;
}

/* 主程序 */
int main() {
    printf("Loading plugin: ./lib/mylib.dylib\n");
    
    /* 加载动态库 */
    void* handle = cosmo_dlopen("./lib/mylib.dylib", RTLD_NOW);
    if (!handle) {
        printf("Failed to load plugin: %s\n", cosmo_dlerror());
        return 1;
    }
    printf("Plugin loaded successfully\n");

    /* 获取函数指针 */
    int (*func)(int) = cosmo_dlsym(handle, "test_func");
    if (!func) {
        printf("Failed to find function: %s\n", cosmo_dlerror());
        cosmo_dlclose(handle);
        return 1;
    }

    printf("Calling test_func...\n");
    int result = func(21);
    printf("Result: %d\n", result);

    cosmo_dlclose(handle);
    return 0;
} 