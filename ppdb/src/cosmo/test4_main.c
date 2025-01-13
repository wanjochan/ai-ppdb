#include "cosmopolitan.h"

typedef int (*test4_add_t)(int, int);
typedef const char* (*test4_version_t)(void);
typedef int (*module_main_t)(void);

int main(int argc, char *argv[]) {
    void* handle;
    test4_add_t add_func;
    test4_version_t version_func;
    module_main_t init_func;
    char buf[256];
    int len;
    
    // 加载动态库
    handle = cosmo_dlopen("./test4.dl", RTLD_NOW);
    if (!handle) {
        write(1, "Failed to load test4.dl\n", 23);
        return 1;
    }
    
    // 获取初始化函数
    init_func = (module_main_t)cosmo_dlsym(handle, "module_main");
    if (init_func) {
        write(1, "Calling module_main...\n", 22);
        init_func();
    } else {
        write(1, "Warning: module_main not found\n", 30);
    }
    
    // 获取并测试函数
    add_func = (test4_add_t)cosmo_dlsym(handle, "test4_add");
    if (add_func) {
        int result = add_func(5, 3);
        len = snprintf(buf, sizeof(buf), "test4_add(5, 3) = %d\n", result);
        write(1, buf, len);
    } else {
        write(1, "Failed to get test4_add\n", 23);
    }
    
    version_func = (test4_version_t)cosmo_dlsym(handle, "test4_version");
    if (version_func) {
        const char* version = version_func();
        len = snprintf(buf, sizeof(buf), "Version: %s\n", version);
        write(1, buf, len);
    } else {
        write(1, "Failed to get test4_version\n", 27);
    }
    
    // 卸载动态库
    cosmo_dlclose(handle);
    write(1, "test4.dl unloaded\n", 18);
    
    return 0;
} 