#include "cosmopolitan.h"

/* 包装函数 */
void* __wrap_ape_stack_round(void* p) {
    return p;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <ape_file>\n", argv[0]);
        return 1;
    }

    printf("Executing APE file: %s\n", argv[1]);
    
    /* 准备参数 */
    char* const args[] = {argv[1], NULL};
    char* const envp[] = {NULL};
    
    /* 直接执行目标程序 */
    execve(argv[1], args, envp);
    
    /* 如果execve返回，说明出错了 */
    printf("Failed to execute %s\n", argv[1]);
    return 1;
} 