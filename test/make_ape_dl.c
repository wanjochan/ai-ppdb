#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ELF64 header
struct Elf64_Ehdr {
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

// PE header (simplified)
#pragma pack(push, 1)
struct Pe_Header {
    uint16_t magic;          // MZ signature
    uint8_t  stub[0x3c-2];   // DOS stub
    uint32_t pe_offset;      // Offset to PE header
    char     pe_sig[4];      // PE\0\0 signature
    uint16_t machine;        // Machine type
    uint16_t num_sections;
    uint32_t timestamp;
    uint32_t symbol_table;
    uint32_t num_symbols;
    uint16_t opt_header_size;
    uint16_t characteristics;
};
#pragma pack(pop)

// Mach-O header
struct MachO_Header {
    uint32_t magic;          // MH_MAGIC_64
    uint32_t cputype;        // CPU_TYPE_ARM64
    uint32_t cpusubtype;     // CPU_SUBTYPE_ARM64_ALL
    uint32_t filetype;       // MH_DYLIB
    uint32_t ncmds;          // Number of load commands
    uint32_t sizeofcmds;     // Size of load commands
    uint32_t flags;          // Flags
    uint32_t reserved;       // Reserved
};

// Mach-O Load Command
struct MachO_LoadCmd {
    uint32_t cmd;            // Load command type
    uint32_t cmdsize;        // Size of load command
};

// Mach-O Segment Command 64
struct MachO_SegmentCmd64 {
    uint32_t cmd;            // LC_SEGMENT_64
    uint32_t cmdsize;        // Size of this command
    char     segname[16];    // Segment name
    uint64_t vmaddr;         // Memory address of this segment
    uint64_t vmsize;         // Memory size of this segment
    uint64_t fileoff;        // File offset of this segment
    uint64_t filesize;       // File size of this segment
    uint32_t maxprot;        // Maximum VM protection
    uint32_t initprot;       // Initial VM protection
    uint32_t nsects;         // Number of sections
    uint32_t flags;          // Flags
};

void write_padding(FILE *f, size_t current, size_t align) {
    size_t pad_size = (align - (current % align)) % align;
    if (pad_size > 0) {
        uint8_t *pad = calloc(pad_size, 1);
        fwrite(pad, 1, pad_size, f);
        free(pad);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.dylib> <output>\n", argv[0]);
        return 1;
    }

    FILE *fin = fopen(argv[1], "rb");
    FILE *fout = fopen(argv[2], "wb");
    if (!fin || !fout) {
        perror("File open failed");
        return 1;
    }

    // Calculate file size
    fseek(fin, 0, SEEK_END);
    long input_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    // 1. Write ELF .so header
    struct Elf64_Ehdr elf = {0};
    memcpy(elf.e_ident, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0", 16);
    elf.e_type = 3;        // ET_DYN
    elf.e_machine = 183;   // EM_AARCH64
    elf.e_version = 1;
    elf.e_ehsize = sizeof(struct Elf64_Ehdr);
    // 设置程序头表偏移
    elf.e_phoff = sizeof(struct Elf64_Ehdr);
    elf.e_phentsize = 56;  // sizeof(Elf64_Phdr)
    elf.e_phnum = 1;       // 一个程序头
    fwrite(&elf, sizeof(elf), 1, fout);

    // Align to 64 bytes
    write_padding(fout, sizeof(elf), 64);

    // 2. Write PE .dll header
    struct Pe_Header pe = {0};
    pe.magic = 0x5A4D;      // MZ
    pe.pe_offset = 0x40;    // PE header offset
    memcpy(pe.pe_sig, "PE\0\0", 4);
    pe.machine = 0xAA64;    // ARM64
    pe.characteristics = 0x2102;  // DLL, executable, 32-bit
    fwrite(&pe, sizeof(pe), 1, fout);

    // Align to 64 bytes
    write_padding(fout, sizeof(pe), 64);

    // 3. Copy the original dylib file
    char *buf = malloc(4096);
    if (!buf) {
        perror("Memory allocation failed");
        fclose(fin);
        fclose(fout);
        return 1;
    }

    size_t n;
    while ((n = fread(buf, 1, 4096, fin)) > 0) {
        if (fwrite(buf, 1, n, fout) != n) {
            perror("Write failed");
            free(buf);
            fclose(fin);
            fclose(fout);
            return 1;
        }
    }

    free(buf);
    fclose(fin);
    fclose(fout);
    printf("Generated APE-DL file: %s\n", argv[2]);
    return 0;
} 