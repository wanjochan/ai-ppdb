#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

/* Windows错误模式常量 */
#define APE_SEM_FAILCRITICALERRORS     0x0001
#define APE_SEM_NOALIGNMENTFAULTEXCEPT 0x0004
#define APE_SEM_NOGPFAULTERRORBOX      0x0002
#define APE_SEM_NOOPENFILEERRORBOX     0x8000

/* 禁用Windows错误弹窗 */
static void disable_error_dialogs(void) {
#if defined(__COSMOPOLITAN__)
    SetErrorMode(APE_SEM_FAILCRITICALERRORS | 
                APE_SEM_NOGPFAULTERRORBOX | 
                APE_SEM_NOOPENFILEERRORBOX);
#endif
}

int main(void) {
    void* handle;
    const char* error;
    const char* libname = "./test4.dll";
    
    /* 禁用错误弹窗 */
    disable_error_dialogs();
    
    // 打印当前工作目录
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        dprintf(1, "Current working directory: %s\n", cwd);
    }
    
    dprintf(1, "Attempting to load: %s\n", libname);
    
    // 使用cosmo_dlopen加载
    handle = cosmo_dlopen(libname, RTLD_NOW);
    if (!handle) {
        error = cosmo_dlerror();
        dprintf(2, "Failed to load %s using cosmo_dlopen: %s\n", 
               libname, error ? error : "Unknown error");
        return 1;
    }
    
    dprintf(1, "Successfully loaded %s\n", libname);
    
    // 获取并调用导出函数
    int (*test4_func)(void);
    test4_func = (int (*)(void))cosmo_dlsym(handle, "test4_func");
    
    if (test4_func) {
        int result = test4_func();
        dprintf(1, "test4_func() returned: %d\n", result);
    } else {
        error = cosmo_dlerror();
        dprintf(2, "Failed to get test4_func: %s\n", 
               error ? error : "Unknown error");
        cosmo_dlclose(handle);
        return 1;
    }
    
    // 卸载动态库
    cosmo_dlclose(handle);
    dprintf(1, "%s unloaded\n", libname);
    
    return 0;
} 