#include "ape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    FILE* fp = fopen("test_dll.dll", "rb");
    if (!fp) {
        printf("Failed to open test_dll.dll\n");
        return 1;
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    size_t code_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // 读取代码
    unsigned char* code = malloc(code_size);
    if (!code) {
        printf("Failed to allocate memory\n");
        fclose(fp);
        return 1;
    }

    if (fread(code, 1, code_size, fp) != code_size) {
        printf("Failed to read code\n");
        free(code);
        fclose(fp);
        return 1;
    }

    fclose(fp);

    // 创建生成器
    struct APE_Generator* gen = ape_create();
    if (!gen) {
        printf("Failed to create generator\n");
        free(code);
        return 1;
    }

    // 设置 DLL 名称
    if (ape_set_dll_name(gen, "test.dat") < 0) {
        printf("Failed to set DLL name\n");
        free(code);
        ape_free_generator(gen);
        return 1;
    }

    // 添加各个头部
    if (ape_add_dos_header(gen) < 0 ||
        ape_add_elf_header(gen) < 0 ||
        ape_add_pe_header(gen) < 0 ||
        ape_add_pe_optional_header(gen) < 0 ||
        ape_add_pe_sections(gen) < 0) {
        printf("Failed to add headers\n");
        free(code);
        ape_free_generator(gen);
        return 1;
    }

    // 添加代码
    if (ape_add_code(gen, code, code_size) < 0) {
        printf("Failed to add code\n");
        free(code);
        ape_free_generator(gen);
        return 1;
    }

    // 添加导出
    if (ape_add_export(gen, "module_main", 0x4000) < 0 ||
        ape_add_export(gen, "test_func1", 0x4100) < 0 ||
        ape_add_export(gen, "test_func2", 0x4200) < 0) {
        printf("Failed to add exports\n");
        free(code);
        ape_free_generator(gen);
        return 1;
    }

    // 添加导出目录
    if (ape_add_pe_exports(gen) < 0) {
        printf("Failed to add export directory\n");
        free(code);
        ape_free_generator(gen);
        return 1;
    }

    // 写入文件
    if (ape_write_file(gen, "test.dat") < 0) {
        printf("Failed to write file\n");
        free(code);
        ape_free_generator(gen);
        return 1;
    }

    printf("Successfully created test.dat\n");

    // 释放资源
    free(code);
    ape_free_generator(gen);
    return 0;
} 