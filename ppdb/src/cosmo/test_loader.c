#include "cosmopolitan.h"
#include "ape_defs.h"
#include "ape/ape.h"
#include "libc/elf/elf.h"
#include "libc/runtime/runtime.h"

int main(int argc, char* argv[]) {
    char target[] = "test_target.exe";
    union ElfEhdrBuf ebuf;
    struct ApeLoader M;
    long sp[2] = {0, 0};
    long auxv[2] = {0, 0};
    
    // 打开目标文件
    int fd = open(target, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open file: %s\n", target);
        return 1;
    }
    
    // 读取 ELF 头部
    if (read(fd, ebuf.buf, sizeof(ebuf.buf)) != sizeof(ebuf.buf)) {
        printf("Failed to read ELF header\n");
        close(fd);
        return 1;
    }
    
    // 使用 TryElf 验证和处理 ELF 头部
    const char* error = TryElf(&M, &ebuf, target, fd, sp, auxv, 4096, 1);
    if (error) {
        printf("TryElf failed: %s\n", error);
        close(fd);
        return 1;
    }
    
    printf("ELF header verified successfully\n");
    close(fd);
    return 0;
}

/* APE入口点 */
void _start(void) {
    static char* argv[] = {"test_loader.com", "test_target.com", NULL};
    exit(main(2, argv));
} 
