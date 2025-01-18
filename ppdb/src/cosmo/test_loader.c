#include "cosmopolitan.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/auxv.h"

// auxv常量定义
#define AT_NULL   0
#define AT_IGNORE 1
#define AT_EXECFD 2
#define AT_PHDR   3
#define AT_PHENT  4
#define AT_PHNUM  5
#define AT_PAGESZ 6
#define AT_BASE   7
#define AT_FLAGS  8
#define AT_ENTRY  9

// ApeLoader的声明
extern void* ApeLoader(const char* path, long* sp, long* auxv, long pagesz, int os);

int main(int argc, char* argv[]) {
    printf("test_loader starting...\n");
    
    if (argc < 2) {
        printf("Usage: %s <target_exe>\n", argv[0]);
        return 1;
    }

    ShowCrashReports();
    printf("Loading target: %s\n", argv[1]);

    // 打开并映射文件
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("Failed to open file: %s\n", argv[1]);
        return 1;
    }
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Failed to stat file\n");
        close(fd);
        return 1;
    }
    printf("File size: %ld bytes\n", st.st_size);
    
    void* base = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (base == MAP_FAILED) {
        printf("Failed to mmap file\n");
        return 1;
    }
    printf("Mapped at address: %p\n", base);

    // 检查APE头
    if (memcmp(base, "MZqFpD", 6) != 0) {
        printf("Invalid APE magic\n");
        munmap(base, st.st_size);
        return 1;
    }
    printf("Found valid APE magic\n");

    // 分配栈空间
    size_t stack_size = 65536;
    void* stack_mem = memalign(16, stack_size);
    if (!stack_mem) {
        printf("Failed to allocate stack\n");
        munmap(base, st.st_size);
        return 1;
    }
    memset(stack_mem, 0, stack_size);
    printf("Stack allocated at: %p\n", stack_mem);
    
    // 设置栈指针
    long* sp = (long*)((char*)stack_mem + stack_size);
    sp = (long*)((uintptr_t)sp & ~15ULL);  // 16字节对齐
    sp -= 32;  // 为栈帧预留空间
    printf("Stack pointer: %p\n", sp);
    
    // 设置参数
    char* target_path = argv[1];
    char* target_argv[] = {target_path, NULL};
    char* target_envp[] = {NULL};
    
    // 计算auxv的位置
    long* auxv = sp + 4;  // 跳过argc, argv[0], NULL, envp[0]
    
    // 设置栈布局
    sp[0] = 1;  // argc
    sp[1] = (long)target_path;  // argv[0]
    sp[2] = 0;  // argv end
    sp[3] = 0;  // envp start
    
    // 设置auxv
    auxv[0] = AT_PAGESZ;
    auxv[1] = 65536;
    auxv[2] = AT_BASE;
    auxv[3] = (long)base;
    auxv[4] = AT_ENTRY;
    auxv[5] = (long)((char*)base + 4096);  // APE头后的代码段
    auxv[6] = AT_NULL;
    auxv[7] = 0;
    
    printf("Calling ApeLoader with:\n");
    printf("  path = %s\n", target_path);
    printf("  sp = %p\n", sp);
    printf("  auxv = %p\n", auxv);
    printf("  pagesz = %d\n", 65536);
    printf("  os = %d\n", 0);
    
    // 调用ApeLoader
    return (int)(long)ApeLoader(target_path, sp, auxv, 65536, 0);
}
