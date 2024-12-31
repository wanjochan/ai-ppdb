#include "ape.h"
#include <stdio.h>

// 测试用的代码段
static const unsigned char test_code[] = {
    0x55,                   // push rbp
    0x48, 0x89, 0xE5,      // mov rbp, rsp
    0xB8, 0x2A, 0x00, 0x00, 0x00,  // mov eax, 42
    0x5D,                   // pop rbp
    0xC3                    // ret
};

int main() {
    APE_Generator* gen = ape_create_generator();
    if (!gen) {
        printf("Failed to create generator\n");
        return 1;
    }
    
    // 添加各种头部
    if (ape_add_dos_header(gen) != 0) {
        printf("Failed to add DOS header\n");
        goto error;
    }
    
    if (ape_add_elf_header(gen) != 0) {
        printf("Failed to add ELF header\n");
        goto error;
    }
    
    if (ape_add_pe_header(gen) != 0) {
        printf("Failed to add PE header\n");
        goto error;
    }
    
    // 添加测试代码
    if (ape_add_code(gen, test_code, sizeof(test_code)) != 0) {
        printf("Failed to add code\n");
        goto error;
    }
    
    // 写入文件
    if (ape_write_file(gen, "test.dll") != 0) {
        printf("Failed to write file\n");
        goto error;
    }
    
    printf("Successfully created test.dll\n");
    ape_free_generator(gen);
    return 0;
    
error:
    ape_free_generator(gen);
    return 1;
} 