#include "cosmopolitan.h"
//#include <windows.h>

// Windows API 类型定义
typedef int BOOL;
typedef unsigned long DWORD;
typedef void* HANDLE;
typedef HANDLE HMODULE;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef void* LPVOID;
typedef unsigned long* LPDWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef BYTE* LPBYTE;
typedef unsigned long long ULONGLONG;  // 移动到这里

#define FALSE 0
#define TRUE 1
#define INFINITE 0xFFFFFFFF
#define MAX_PATH 260

typedef struct _STARTUPINFOA {
    DWORD  cb;
    LPSTR  lpReserved;
    LPSTR  lpDesktop;
    LPSTR  lpTitle;
    DWORD  dwX;
    DWORD  dwY;
    DWORD  dwXSize;
    DWORD  dwYSize;
    DWORD  dwXCountChars;
    DWORD  dwYCountChars;
    DWORD  dwFillAttribute;
    DWORD  dwFlags;
    WORD   wShowWindow;
    WORD   cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFO, *LPSTARTUPINFO;

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
} PROCESS_INFORMATION, *LPPROCESS_INFORMATION;

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

typedef struct _IMAGE_DATA_DIRECTORY {
    uint32_t VirtualAddress;
    uint32_t Size;
} IMAGE_DATA_DIRECTORY;

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
    IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS64 {
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64;

typedef struct _IMAGE_SECTION_HEADER {
    uint8_t  Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} IMAGE_SECTION_HEADER;

#define IMAGE_SCN_MEM_EXECUTE 0x20000000
#define IMAGE_SCN_MEM_READ    0x40000000
#define IMAGE_SCN_MEM_WRITE   0x80000000

typedef struct _IMAGE_IMPORT_DESCRIPTOR {
    union {
        DWORD Characteristics;
        DWORD OriginalFirstThunk;
    };
    DWORD TimeDateStamp;
    DWORD ForwarderChain;
    DWORD Name;
    DWORD FirstThunk;
} IMAGE_IMPORT_DESCRIPTOR;

typedef struct _IMAGE_THUNK_DATA64 {
    union {
        ULONGLONG ForwarderString;
        ULONGLONG Function;
        ULONGLONG Ordinal;
        ULONGLONG AddressOfData;
    } u1;
} IMAGE_THUNK_DATA64;

typedef struct _IMAGE_IMPORT_BY_NAME {
    WORD Hint;
    BYTE Name[1];
} IMAGE_IMPORT_BY_NAME;

#define IMAGE_ORDINAL_FLAG64 0x8000000000000000ULL
#define IMAGE_ORDINAL64(Ordinal) (Ordinal & 0xFFFF)

int process_imports(void* imageBase, IMAGE_NT_HEADERS64* ntHeaders) {
    IMAGE_DATA_DIRECTORY* importDir = &ntHeaders->OptionalHeader.DataDirectory[1];
    if (importDir->Size == 0) {
        printf("No imports\n");
        return 0;
    }

    IMAGE_IMPORT_DESCRIPTOR* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)((char*)imageBase + 
        importDir->VirtualAddress);

    printf("Processing imports...\n");
    for (; importDesc->Name; importDesc++) {
        char* dllName = (char*)imageBase + importDesc->Name;
        printf("Loading DLL: %s\n", dllName);

        int64_t dllBase = (int64_t)LoadLibraryA(dllName);  // 修改类型转换
        if (!dllBase) {
            printf("Failed to load DLL: %s\n", dllName);
            return 1;
        }

        IMAGE_THUNK_DATA64* thunk = (IMAGE_THUNK_DATA64*)((char*)imageBase + 
            importDesc->FirstThunk);
        IMAGE_THUNK_DATA64* origThunk = (IMAGE_THUNK_DATA64*)((char*)imageBase + 
            importDesc->OriginalFirstThunk);

        for (; origThunk->u1.Function; origThunk++, thunk++) {
            void* funcAddr;
            if (origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) {
                WORD ordinal = IMAGE_ORDINAL64(origThunk->u1.Ordinal);
                funcAddr = GetProcAddress(dllBase, (const char*)(uintptr_t)ordinal);
                printf("  Imported by ordinal: %d\n", ordinal);
            } else {
                IMAGE_IMPORT_BY_NAME* importByName = (IMAGE_IMPORT_BY_NAME*)((char*)imageBase + 
                    origThunk->u1.AddressOfData);
                funcAddr = GetProcAddress(dllBase, (const char*)importByName->Name);
                printf("  Imported by name: %s\n", importByName->Name);
            }

            if (!funcAddr) {
                printf("Failed to get function address\n");
                return 1;
            }

            thunk->u1.Function = (ULONGLONG)(uintptr_t)funcAddr;  // 修改类型转换
        }
    }

    return 0;
}

typedef struct _IMAGE_BASE_RELOCATION {
    DWORD VirtualAddress;
    DWORD SizeOfBlock;
} IMAGE_BASE_RELOCATION;

#define IMAGE_REL_BASED_DIR64 10

int process_relocations(void* imageBase, IMAGE_NT_HEADERS64* ntHeaders, uint64_t delta) {
    if (delta == 0) {
        printf("No relocation needed\n");
        return 0;
    }

    IMAGE_DATA_DIRECTORY* relocDir = &ntHeaders->OptionalHeader.DataDirectory[5];
    if (relocDir->Size == 0) {
        printf("No relocations\n");
        return 0;
    }

    printf("Processing relocations...\n");
    IMAGE_BASE_RELOCATION* reloc = (IMAGE_BASE_RELOCATION*)((char*)imageBase + 
        relocDir->VirtualAddress);

    while (reloc->VirtualAddress) {
        WORD* relocData = (WORD*)((char*)reloc + sizeof(IMAGE_BASE_RELOCATION));
        int numRelocations = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);

        for (int i = 0; i < numRelocations; i++) {
            WORD relocEntry = relocData[i];
            WORD type = (relocEntry >> 12) & 0xF;
            WORD offset = relocEntry & 0xFFF;

            if (type == IMAGE_REL_BASED_DIR64) {
                uint64_t* address = (uint64_t*)((char*)imageBase + reloc->VirtualAddress + offset);
                *address += delta;
            }
        }

        reloc = (IMAGE_BASE_RELOCATION*)((char*)reloc + reloc->SizeOfBlock);
    }

    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <target>\n", argv[0]);
        return 1;
    }

    // 1. 打开文件
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("Failed to open file: %s\n", argv[1]);
        return 1;
    }

    // 2. 获取文件大小
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Failed to get file size\n");
        close(fd);
        return 1;
    }

    // 3. 读取文件内容到临时缓冲区
    void* fileBuffer = malloc(st.st_size);
    if (!fileBuffer) {
        printf("Failed to allocate memory for file\n");
        close(fd);
        return 1;
    }

    if (read(fd, fileBuffer, st.st_size) != st.st_size) {
        printf("Failed to read file\n");
        free(fileBuffer);
        close(fd);
        return 1;
    }

    close(fd);

    // 4. 检查 DOS 头
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)fileBuffer;
    if (dosHeader->e_magic != 0x5A4D) { // "MZ"
        printf("Invalid DOS signature\n");
        free(fileBuffer);
        return 1;
    }

    printf("DOS Header OK\n");

    // 5. 获取 PE 头
    IMAGE_NT_HEADERS64* ntHeaders = (IMAGE_NT_HEADERS64*)((char*)fileBuffer + dosHeader->e_lfanew);
    if (ntHeaders->Signature != 0x4550) { // "PE\0\0"
        printf("Invalid PE signature\n");
        free(fileBuffer);
        return 1;
    }

    printf("PE Header OK\n");
    printf("Entry Point RVA: 0x%x\n", ntHeaders->OptionalHeader.AddressOfEntryPoint);
    printf("Image Base: 0x%llx\n", ntHeaders->OptionalHeader.ImageBase);
    printf("Size of Image: 0x%x\n", ntHeaders->OptionalHeader.SizeOfImage);

    // 6. 分配内存
    void* imageBase = mmap((void*)ntHeaders->OptionalHeader.ImageBase,
                          ntHeaders->OptionalHeader.SizeOfImage,
                          PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1, 0);

    if (imageBase == MAP_FAILED) {
        printf("Failed to allocate memory at preferred base. Trying anywhere...\n");
        imageBase = mmap(NULL, ntHeaders->OptionalHeader.SizeOfImage,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
        if (imageBase == MAP_FAILED) {
            printf("Failed to allocate memory for image\n");
            free(fileBuffer);
            return 1;
        }
    }

    printf("Allocated memory at: %p\n", imageBase);

    // 7. 复制头部
    memcpy(imageBase, fileBuffer, ntHeaders->OptionalHeader.SizeOfHeaders);

    // 8. 加载节
    IMAGE_SECTION_HEADER* sections = (IMAGE_SECTION_HEADER*)((char*)ntHeaders + 
        sizeof(IMAGE_NT_HEADERS64));

    printf("Loading %d sections:\n", ntHeaders->FileHeader.NumberOfSections);
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        printf("Section %d: VA=0x%x Size=0x%x\n", 
               i, sections[i].VirtualAddress, sections[i].VirtualSize);

        if (sections[i].SizeOfRawData > 0) {
            void* dest = (char*)imageBase + sections[i].VirtualAddress;
            void* src = (char*)fileBuffer + sections[i].PointerToRawData;
            memcpy(dest, src, sections[i].SizeOfRawData);

            // 设置节权限
            int prot = PROT_READ;  // 默认可读
            if (sections[i].Characteristics & IMAGE_SCN_MEM_WRITE)   prot |= PROT_WRITE;
            if (sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) prot |= PROT_EXEC;

            if (mprotect((void*)((uintptr_t)dest & ~0xFFF), 
                        (sections[i].VirtualSize + 0xFFF) & ~0xFFF, 
                        prot) != 0) {
                printf("Warning: Failed to set section %d permissions\n", i);
            }
        }
    }

    free(fileBuffer);

    // 计算重定位增量
    uint64_t delta = (uint64_t)imageBase - ntHeaders->OptionalHeader.ImageBase;

    // 处理重定位
    if (process_relocations(imageBase, ntHeaders, delta) != 0) {
        printf("Failed to process relocations\n");
        munmap(imageBase, ntHeaders->OptionalHeader.SizeOfImage);
        return 1;
    }

    // 处理导入表
    if (process_imports(imageBase, ntHeaders) != 0) {
        printf("Failed to process imports\n");
        munmap(imageBase, ntHeaders->OptionalHeader.SizeOfImage);
        return 1;
    }

    // 获取入口点
    void* entry = (char*)imageBase + ntHeaders->OptionalHeader.AddressOfEntryPoint;
    printf("Entry point at: %p\n", entry);

    // 执行入口点
    typedef int (*DllMain)(void* hinstDLL, unsigned long fdwReason, void* lpvReserved);
    DllMain ep = (DllMain)entry;
    
    printf("Executing...\n");
    int result = ep(imageBase, 1, NULL);  // 1 = DLL_PROCESS_ATTACH
    printf("Execution result: %d\n", result);

    // 清理
    munmap(imageBase, ntHeaders->OptionalHeader.SizeOfImage);
    return result;
}
