#ifndef APE_LOADER_H
#define APE_LOADER_H

#include "cosmopolitan.h"

/* 基本类型定义 */
#ifndef bool
typedef int bool;
#define true 1
#define false 0
#endif

/* APE 头部结构 */
struct ApeHeader {
    uint64_t mz_magic;      /* DOS MZ 魔数 */
    uint8_t  pad1[0x3c];
    uint32_t pe_magic;      /* PE 魔数 */
    uint16_t machine;       /* AMD64 */
    uint16_t num_sections;
    uint32_t timestamp;
    uint8_t  pad2[0x40];
    uint32_t elf_magic;     /* ELF 魔数 */
    uint8_t  elf_class;     /* 64位 */
    uint8_t  elf_data;      /* 小端 */
    uint8_t  elf_version;   /* 版本1 */
    uint8_t  elf_abi;       /* System V */
    uint64_t elf_pad;
    uint16_t elf_type;      /* ET_DYN */
    uint16_t elf_machine;   /* x86-64 */
    uint32_t elf_version2;  /* 版本1 */
    uint64_t elf_entry;     /* 入口点 */
    uint8_t  pad3[0x40];
    uint32_t macho_magic;   /* Mach-O 魔数 */
    uint32_t macho_cputype;
    uint32_t macho_cpusubtype;
    uint32_t macho_filetype;
    uint32_t macho_ncmds;
    uint32_t macho_sizeofcmds;
    uint32_t macho_flags;
    uint32_t macho_reserved;
};

/* APE 加载器函数 */
void* ape_load(const char* path);
void* ape_get_proc(void* handle, const char* symbol);
int ape_unload(void* handle);

#endif /* APE_LOADER_H */ 