#include "cosmopolitan.h"
#include "ape/ape.h"
#include "libc/elf/elf.h"
#include "libc/runtime/runtime.h"

// ApeLoader的声明
extern __attribute__((__noreturn__)) void ApeLoader(long di, long *sp, char dl);

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
    
    // 准备完整的栈布局
    // sp[0] = argc
    // sp[1...argc] = argv
    // sp[argc+1] = NULL
    // sp[argc+2] = envp[0] = NULL
    // sp[argc+3] = auxv[0] = AT_PAGESZ
    // sp[argc+4] = auxv[1] = 4096
    // sp[argc+5] = 0 (auxv end)
    long sp[256] = {0};  // 足够大的栈空间
    sp[0] = argc - 1;  // 新的argc
    
    // 设置argv
    char** new_argv = argv + 1;  // 跳过loader自身
    for (int i = 0; i < argc - 1; i++) {
        sp[i + 1] = (long)new_argv[i];
    }
    sp[argc] = 0;  // argv结束标记
    
    // 设置envp
    sp[argc + 1] = 0;  // envp结束标记
    
    // 设置auxv
    sp[argc + 2] = AT_PAGESZ;
    sp[argc + 3] = 4096;
    sp[argc + 4] = 0;  // auxv结束标记
    
    // 检查栈指针和参数设置
    printf("Stack setup before ApeLoader:\n");
    for (int i = 0; i < argc + 5; i++) {
        printf("  sp[%d] = %lx\n", i, sp[i]);
    }
    
    // 检查栈指针对齐
    if ((long)sp & 15) {
        printf("Warning: Stack pointer is not 16-byte aligned\n");
    }
    
    // 在调用 ApeLoader 之前添加更多日志输出
    printf("Checking stack alignment and values before ApeLoader call:\n");
    printf("  sp alignment: %s\n", ((long)sp & 15) ? "Not aligned" : "Aligned");
    for (int i = 0; i < argc + 5; i++) {
        printf("  sp[%d] = %lx\n", i, sp[i]);
    }
    
    // 检查目标文件是否可执行
    if (access(argv[1], X_OK) != 0) {
        perror("Error: Target executable is not accessible or not executable");
        return 1;
    }
    
    // 调用ApeLoader
    // di = argc-1 (新的参数个数)
    // sp = 栈指针
    // dl = 0 (默认为LINUX)
    ApeLoader(argc - 1, sp, 0);
    __builtin_unreachable();  // ApeLoader不会返回
}
