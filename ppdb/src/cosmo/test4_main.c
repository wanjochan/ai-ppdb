#include "cosmopolitan.h"

int main(int argc, char *argv[]) {
    void* handle;
    
    // 加载动态库
    handle = cosmo_dlopen("./test4.dl", RTLD_NOW);
    if (!handle) {
        write(2, "Failed to load test4.dl\n", 23);
        return 1;
    }
    
    write(1, "Successfully loaded test4.dl\n", 28);
    
    // 卸载动态库
    cosmo_dlclose(handle);
    write(1, "test4.dl unloaded\n", 18);
    
    return 0;
} 