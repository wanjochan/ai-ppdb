#ifndef _APE_H_
#define _APE_H_

#include <stdint.h>

#pragma pack(push, 1)

// DOS MZ Header
struct DOS_Header {
    uint16_t e_magic;    // MZ signature
    uint16_t e_cblp;     // Bytes on last page of file
    uint16_t e_cp;       // Pages in file
    uint16_t e_crlc;     // Relocations
    uint16_t e_cparhdr;  // Size of header in paragraphs
    uint16_t e_minalloc; // Minimum extra paragraphs needed
    uint16_t e_maxalloc; // Maximum extra paragraphs needed
    uint16_t e_ss;       // Initial (relative) SS value
    uint16_t e_sp;       // Initial SP value
    uint16_t e_csum;     // Checksum
    uint16_t e_ip;       // Initial IP value
    uint16_t e_cs;       // Initial (relative) CS value
    uint16_t e_lfarlc;   // File address of relocation table
    uint16_t e_ovno;     // Overlay number
    uint16_t e_res[4];   // Reserved words
    uint16_t e_oemid;    // OEM identifier
    uint16_t e_oeminfo;  // OEM information
    uint16_t e_res2[10]; // Reserved words
    uint32_t e_lfanew;   // File address of new exe header
};

// ELF Header
struct ELF_Header {
    unsigned char e_ident[16];  // Magic number and other info
    uint16_t e_type;           // Object file type
    uint16_t e_machine;        // Architecture
    uint32_t e_version;        // Object file version
    uint64_t e_entry;          // Entry point virtual address
    uint64_t e_phoff;          // Program header table file offset
    uint64_t e_shoff;          // Section header table file offset
    uint32_t e_flags;          // Processor-specific flags
    uint16_t e_ehsize;         // ELF header size in bytes
    uint16_t e_phentsize;      // Program header table entry size
    uint16_t e_phnum;          // Program header table entry count
    uint16_t e_shentsize;      // Section header table entry size
    uint16_t e_shnum;          // Section header table entry count
    uint16_t e_shstrndx;       // Section header string table index
};

// PE Header
struct PE_Header {
    uint32_t Signature;        // PE\0\0 signature
    uint16_t Machine;          // Target machine
    uint16_t NumberOfSections; // Number of sections
    uint32_t TimeDateStamp;    // Time stamp
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

// APE Header Generator
struct APE_Generator {
    // 头部缓冲区
    unsigned char* buffer;
    size_t size;
    
    // 各个头部的偏移
    size_t dos_offset;
    size_t elf_offset;
    size_t pe_offset;
    
    // 代码段偏移
    size_t code_offset;
};

// 函数声明
APE_Generator* ape_create_generator(void);
void ape_free_generator(APE_Generator* gen);
int ape_add_dos_header(APE_Generator* gen);
int ape_add_elf_header(APE_Generator* gen);
int ape_add_pe_header(APE_Generator* gen);
int ape_add_code(APE_Generator* gen, const void* code, size_t size);
int ape_write_file(APE_Generator* gen, const char* filename);

#pragma pack(pop)

#endif // _APE_H_ 