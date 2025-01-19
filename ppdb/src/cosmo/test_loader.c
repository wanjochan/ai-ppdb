#include "cosmopolitan.h"
//#include <windows.h>

//EXTERN_C
__attribute__((__noreturn__)) void ApeLoader(long di, long *sp, char dl);


#define READ32_BE(S)                                                      \
  ((unsigned)(255 & (S)[0]) << 030 | (unsigned)(255 & (S)[1]) << 020 | \
   (unsigned)(255 & (S)[2]) << 010 | (unsigned)(255 & (S)[3]) << 000)

#define READ64_BE(S)                                                                \
  ((uint64_t)(255 & (S)[0]) << 070 | (uint64_t)(255 & (S)[1]) << 060 |           \
   (uint64_t)(255 & (S)[2]) << 050 | (uint64_t)(255 & (S)[3]) << 040 |           \
   (uint64_t)(255 & (S)[4]) << 030 | (uint64_t)(255 & (S)[5]) << 020 |           \
   (uint64_t)(255 & (S)[6]) << 010 | (uint64_t)(255 & (S)[7]) << 000)

#define READ64(S)                         \
  ((unsigned long)(255 & (S)[7]) << 070 | \
   (unsigned long)(255 & (S)[6]) << 060 | \
   (unsigned long)(255 & (S)[5]) << 050 | \
   (unsigned long)(255 & (S)[4]) << 040 | \
   (unsigned long)(255 & (S)[3]) << 030 | \
   (unsigned long)(255 & (S)[2]) << 020 | \
   (unsigned long)(255 & (S)[1]) << 010 | \
   (unsigned long)(255 & (S)[0]) << 000)

// PE 文件格式相关结构
typedef struct _IMAGE_DOS_HEADER {
    uint16_t e_magic;    // Magic number (MZ)
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
} IMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} IMAGE_FILE_HEADER;

typedef struct _IMAGE_OPTIONAL_HEADER64 {
    uint16_t Magic;
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
} IMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS64 {
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64;

int load_and_run(const char* filename) {
    // 1. 打开文件
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open file: %s\n", filename);
        return 1;
    }

    // 2. 获取文件大小
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Failed to get file size\n");
        close(fd);
        return 1;
    }

    // 3. 映射文件到内存
    void* base = mmap(NULL, st.st_size, 
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE, fd, 0);
    
    if (base == MAP_FAILED) {
        printf("Failed to map file into memory\n");
        close(fd);
        return 1;
    }

    close(fd);

    // 4. 检查 DOS 头
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)base;
    if (dosHeader->e_magic != 0x5A4D) { // "MZ"
        printf("Invalid DOS signature\n");
        munmap(base, st.st_size);
        return 1;
    }

    // 5. 获取 PE 头
    IMAGE_NT_HEADERS64* ntHeaders = (IMAGE_NT_HEADERS64*)((char*)base + dosHeader->e_lfanew);
    if (ntHeaders->Signature != 0x4550) { // "PE\0\0"
        printf("Invalid PE signature\n");
        munmap(base, st.st_size);
        return 1;
    }

    // 6. 获取入口点
    uint32_t entryPoint = ntHeaders->OptionalHeader.AddressOfEntryPoint;
    void* entry = (char*)base + entryPoint;

    printf("File mapped at: %p\n", base);
    printf("Entry point at: %p (offset: 0x%x)\n", entry, entryPoint);

    // 7. 执行入口点
    typedef void (*EntryPoint)();
    EntryPoint ep = (EntryPoint)entry;
    ep();

    // 8. 清理
    munmap(base, st.st_size);
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <target>\n", argv[0]);
        return 1;
    }
    return load_and_run(argv[1]);
}
