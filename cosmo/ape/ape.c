#include "ape.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define APE_INITIAL_SIZE 4096
#define DOS_HEADER_OFFSET 0x0000
#define ELF_HEADER_OFFSET 0x0040
#define PE_HEADER_OFFSET 0x0080
#define CODE_OFFSET 0x1000

// 创建生成器
APE_Generator* ape_create_generator(void) {
    APE_Generator* gen = (APE_Generator*)malloc(sizeof(APE_Generator));
    if (!gen) return NULL;
    
    gen->buffer = (unsigned char*)malloc(APE_INITIAL_SIZE);
    if (!gen->buffer) {
        free(gen);
        return NULL;
    }
    
    gen->size = APE_INITIAL_SIZE;
    gen->dos_offset = DOS_HEADER_OFFSET;
    gen->elf_offset = ELF_HEADER_OFFSET;
    gen->pe_offset = PE_HEADER_OFFSET;
    gen->code_offset = CODE_OFFSET;
    
    memset(gen->buffer, 0, gen->size);
    return gen;
}

// 释放生成器
void ape_free_generator(APE_Generator* gen) {
    if (gen) {
        if (gen->buffer) free(gen->buffer);
        free(gen);
    }
}

// 添加 DOS 头部
int ape_add_dos_header(APE_Generator* gen) {
    if (!gen || !gen->buffer) return -1;
    
    struct DOS_Header* dos = (struct DOS_Header*)(gen->buffer + gen->dos_offset);
    dos->e_magic = 0x5A4D;  // "MZ"
    dos->e_lfanew = gen->pe_offset;
    
    // 添加 DOS Stub 程序
    const char* stub = "This program cannot be run in DOS mode.\r\n$";
    memcpy(gen->buffer + sizeof(struct DOS_Header), stub, strlen(stub));
    
    return 0;
}

// 添加 ELF 头部
int ape_add_elf_header(APE_Generator* gen) {
    if (!gen || !gen->buffer) return -1;
    
    struct ELF_Header* elf = (struct ELF_Header*)(gen->buffer + gen->elf_offset);
    
    // ELF 魔数
    unsigned char magic[] = {0x7F, 'E', 'L', 'F', 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(elf->e_ident, magic, sizeof(magic));
    
    elf->e_type = 3;        // ET_DYN
    elf->e_machine = 0x3E;  // EM_X86_64
    elf->e_version = 1;
    elf->e_entry = gen->code_offset;
    
    return 0;
}

// 添加 PE 头部
int ape_add_pe_header(APE_Generator* gen) {
    if (!gen || !gen->buffer) return -1;
    
    struct PE_Header* pe = (struct PE_Header*)(gen->buffer + gen->pe_offset);
    pe->Signature = 0x00004550;  // "PE\0\0"
    pe->Machine = 0x8664;        // IMAGE_FILE_MACHINE_AMD64
    pe->NumberOfSections = 1;    // 只有一个代码段
    pe->Characteristics = 0x2102; // DLL, executable, 32-bit
    
    return 0;
}

// 添加代码段
int ape_add_code(APE_Generator* gen, const void* code, size_t size) {
    if (!gen || !gen->buffer || !code) return -1;
    
    // 确保缓冲区足够大
    size_t required_size = gen->code_offset + size;
    if (required_size > gen->size) {
        size_t new_size = (required_size + 4095) & ~4095;
        unsigned char* new_buffer = (unsigned char*)realloc(gen->buffer, new_size);
        if (!new_buffer) return -1;
        
        gen->buffer = new_buffer;
        gen->size = new_size;
    }
    
    // 复制代码
    memcpy(gen->buffer + gen->code_offset, code, size);
    return 0;
}

// 写入文件
int ape_write_file(APE_Generator* gen, const char* filename) {
    if (!gen || !gen->buffer || !filename) return -1;
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    size_t written = fwrite(gen->buffer, 1, gen->size, fp);
    fclose(fp);
    
    return written == gen->size ? 0 : -1;
} 