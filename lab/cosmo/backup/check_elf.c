#include "cosmopolitan.h"
#include <stdio.h>
#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    unsigned char e_ident[16];    /* Magic number and other info */
    uint16_t      e_type;         /* Object file type */
    uint16_t      e_machine;      /* Architecture */
    uint32_t      e_version;      /* Object file version */
    uint64_t      e_entry;        /* Entry point virtual address */
    uint64_t      e_phoff;        /* Program header table file offset */
    uint64_t      e_shoff;        /* Section header table file offset */
    uint32_t      e_flags;        /* Processor-specific flags */
    uint16_t      e_ehsize;       /* ELF header size in bytes */
    uint16_t      e_phentsize;    /* Program header table entry size */
    uint16_t      e_phnum;        /* Program header table entry count */
    uint16_t      e_shentsize;    /* Section header table entry size */
    uint16_t      e_shnum;        /* Section header table entry count */
    uint16_t      e_shstrndx;     /* Section header string table index */
} Elf64_Ehdr;
#pragma pack(pop)

int main(void) {
    FILE* f = fopen("test4.dll", "rb");
    if (!f) {
        printf("Failed to open test4.dll\n");
        return 1;
    }
    
    Elf64_Ehdr header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        printf("Failed to read ELF header\n");
        fclose(f);
        return 1;
    }
    
    printf("Magic: %02X %02X %02X %02X\n", 
           header.e_ident[0], header.e_ident[1], 
           header.e_ident[2], header.e_ident[3]);
           
    printf("Type: %04X\n", header.e_type);
    printf("Machine: %04X\n", header.e_machine);
    printf("Version: %08X\n", header.e_version);
    printf("Entry: %016llX\n", header.e_entry);
    
    fclose(f);
    return 0;
} 