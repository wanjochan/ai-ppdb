#include "cosmopolitan.h"
#include "ape_loader.h"

/* 定义正确的函数类型 */
typedef int (*target_func_t)(int, int);

int main(int argc, char *argv[]) {
    printf("APE Loader starting...\n");
    
    /* 加载目标文件 */
    void* handle = ape_load("test_target.exe");
    if (!handle) {
        printf("Failed to load target\n");
        return 1;
    }
    
    /* 获取函数地址 */
    //void* func_addr = ape_get_proc(handle, "test_func");
    void* func_addr = ape_get_proc(handle, "main");
    if (!func_addr) {
        printf("Failed to get function address\n");
        ape_unload(handle);
        return 1;
    }
    
    /* 转换为正确的函数类型 */
    target_func_t func = (target_func_t)func_addr;
    
    /* 尝试调用函数 */
    printf("Attempting to call function at %p\n", func);
    
    int arg1 = 42;
    int arg2 = 21;
    printf("Calling with args: %d, %d\n", arg1, arg2);
    
    int result = func(arg1, arg2);
    printf("Function call succeeded with result: %d\n", result);
    
    /* 卸载模块 */
    ape_unload(handle);
    return 0;
}

/* APE入口点 */
void _start(void) {
    static char* argv[] = {"test_loader.com", "test_target.com", NULL};
    exit(main(2, argv));
} 
