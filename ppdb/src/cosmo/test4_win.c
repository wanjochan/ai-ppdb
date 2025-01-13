#include <windows.h>
#include <stdio.h>

typedef int (*TEST4_FUNC)(void);

int main(void) {
    HMODULE hModule;
    TEST4_FUNC test4_func;
    
    // 加载DLL
    hModule = LoadLibraryA("test4.dll");
    if (!hModule) {
        printf("LoadLibrary failed, error: %lu\n", GetLastError());
        return 1;
    }
    printf("DLL loaded successfully at: %p\n", hModule);
    
    // 获取函数地址
    test4_func = (TEST4_FUNC)GetProcAddress(hModule, "test4_func");
    if (!test4_func) {
        printf("GetProcAddress failed, error: %lu\n", GetLastError());
        FreeLibrary(hModule);
        return 1;
    }
    printf("Function address: %p\n", test4_func);
    
    // 调用函数
    int result = test4_func();
    printf("test4_func() returned: %d\n", result);
    
    // 卸载DLL
    FreeLibrary(hModule);
    printf("DLL unloaded\n");
    
    return 0;
} 