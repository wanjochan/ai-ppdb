#include "cosmopolitan.h"
#include "ape/ape.h"
#include "libc/elf/elf.h"
#include "libc/runtime/runtime.h"

// ApeLoader function declaration
int ApeLoader(int argc, char** argv, int flags);

int main(int argc, char* argv[]) {
    printf("test_loader starting...\n");
    printf("Arguments: %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d] = %s\n", i, argv[i]);
    }
    
    if (argc < 2) {
        printf("Usage: %s <target_exe> [args...]\n", argv[0]);
        return 1;
    }
    
    printf("Loading target: %s\n", argv[1]);
    ShowCrashReports();  // 启用崩溃报告
    
    // 准备参数
    char** new_argv = argv + 1;  // 跳过loader自身的名字
    int new_argc = argc - 1;     // 减去loader自身
    
    // 调用ApeLoader
    return ApeLoader(new_argc, new_argv, 0);
} 
