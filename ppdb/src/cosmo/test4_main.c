#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

int main(void) {
    void* handle;
    const char* error;
    const char* libname = "./test4.dll";
    
    // 禁用错误弹窗
    ShowCrashReports();
    
    // 打印当前工作目录
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        dprintf(1, "Current working directory: %s\n", cwd);
    }
    
    dprintf(1, "Attempting to load: %s\n", libname);
    
    // 加载动态库
    handle = cosmo_dlopen(libname, RTLD_NOW);
    if (!handle) {
        error = cosmo_dlerror();
        dprintf(2, "Failed to load %s: %s\n", libname, error ? error : "Unknown error");
        return 1;
    }
    
    dprintf(1, "Successfully loaded %s\n", libname);
    
    // 卸载动态库
    cosmo_dlclose(handle);
    dprintf(1, "%s unloaded\n", libname);
    
    return 0;
} 