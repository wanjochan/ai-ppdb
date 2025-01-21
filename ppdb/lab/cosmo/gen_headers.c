#include "cosmopolitan.h"

// PE/COFF 头部结构
struct PeHeader {
    uint16_t machine;
    uint16_t numberOfSections;
    uint32_t timeDateStamp;
    uint32_t pointerToSymbolTable;
    uint32_t numberOfSymbols;
    uint16_t sizeOfOptionalHeader;
    uint16_t characteristics;
};

// ELF 头部结构
struct ElfHeader {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

// Mach-O 头部结构
struct MachHeader64 {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
};

int main(void) {
    // 生成 PE 头
    struct PeHeader pe = {
        .machine = 0x8664,  // AMD64
        .numberOfSections = 6,
        .characteristics = 0x2102  // DLL, executable
    };
    
    // 生成 ELF 头
    struct ElfHeader elf = {
        .e_ident = {0x7f, 'E', 'L', 'F', 2, 1, 1},
        .e_type = 3,  // ET_DYN
        .e_machine = 62,  // EM_X86_64
        .e_version = 1
    };
    
    // 生成 Mach-O 头
    struct MachHeader64 macho = {
        .magic = 0xfeedfacf,  // MH_MAGIC_64
        .cputype = 0x1000007,  // CPU_TYPE_X86_64
        .filetype = 6,  // MH_DYLIB
        .ncmds = 4
    };
    
    // 写入文件
    FILE *f;
    
    f = fopen("peheader.bin", "wb");
    fwrite(&pe, sizeof(pe), 1, f);
    fclose(f);
    
    f = fopen("elfheader.bin", "wb");
    fwrite(&elf, sizeof(elf), 1, f);
    fclose(f);
    
    f = fopen("machoheader.bin", "wb");
    fwrite(&macho, sizeof(macho), 1, f);
    fclose(f);
    
    return 0;
} 