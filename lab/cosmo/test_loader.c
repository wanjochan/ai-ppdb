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
typedef unsigned long long ULONGLONG;

#define FALSE 0
#define TRUE 1
#define INFINITE 0xFFFFFFFF
#define MAX_PATH 260

// Windows Memory Management
#define MEM_COMMIT      0x00001000
#define MEM_RESERVE     0x00002000
#define MEM_RELEASE     0x00008000
#define PAGE_EXECUTE_READWRITE  0x40

// Function prototypes

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
    WORD e_magic;
    WORD e_cblp;
    WORD e_cp;
    WORD e_crlc;
    WORD e_cparhdr;
    WORD e_minalloc;
    WORD e_maxalloc;
    WORD e_ss;
    WORD e_sp;
    WORD e_csum;
    WORD e_ip;
    WORD e_cs;
    WORD e_lfarlc;
    WORD e_ovno;
    WORD e_res[4];
    WORD e_oemid;
    WORD e_oeminfo;
    WORD e_res2[10];
    DWORD e_lfanew;
} IMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER {
    WORD Machine;
    WORD NumberOfSections;
    DWORD TimeDateStamp;
    DWORD PointerToSymbolTable;
    DWORD NumberOfSymbols;
    WORD SizeOfOptionalHeader;
    WORD Characteristics;
} IMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
    DWORD VirtualAddress;
    DWORD Size;
} IMAGE_DATA_DIRECTORY;

typedef struct _IMAGE_OPTIONAL_HEADER64 {
    WORD Magic;
    BYTE MajorLinkerVersion;
    BYTE MinorLinkerVersion;
    DWORD SizeOfCode;
    DWORD SizeOfInitializedData;
    DWORD SizeOfUninitializedData;
    DWORD AddressOfEntryPoint;
    DWORD BaseOfCode;
    ULONGLONG ImageBase;
    DWORD SectionAlignment;
    DWORD FileAlignment;
    WORD MajorOperatingSystemVersion;
    WORD MinorOperatingSystemVersion;
    WORD MajorImageVersion;
    WORD MinorImageVersion;
    WORD MajorSubsystemVersion;
    WORD MinorSubsystemVersion;
    DWORD Win32VersionValue;
    DWORD SizeOfImage;
    DWORD SizeOfHeaders;
    DWORD CheckSum;
    WORD Subsystem;
    WORD DllCharacteristics;
    ULONGLONG SizeOfStackReserve;
    ULONGLONG SizeOfStackCommit;
    ULONGLONG SizeOfHeapReserve;
    ULONGLONG SizeOfHeapCommit;
    DWORD LoaderFlags;
    DWORD NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS64 {
    DWORD Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64;

typedef struct _IMAGE_SECTION_HEADER {
    BYTE Name[8];
    DWORD VirtualSize;
    DWORD VirtualAddress;
    DWORD SizeOfRawData;
    DWORD PointerToRawData;
    DWORD PointerToRelocations;
    DWORD PointerToLinenumbers;
    WORD NumberOfRelocations;
    WORD NumberOfLinenumbers;
    DWORD Characteristics;
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

typedef struct _IMAGE_BASE_RELOCATION {
    DWORD VirtualAddress;
    DWORD SizeOfBlock;
} IMAGE_BASE_RELOCATION;

#define IMAGE_REL_BASED_DIR64 10

int process_imports(void* imageBase, IMAGE_NT_HEADERS64* ntHeaders) {
    IMAGE_DATA_DIRECTORY* importDir = &ntHeaders->OptionalHeader.DataDirectory[1];
    if (importDir->Size == 0) {
        printf("No imports\n");
        return 0;
    }

    printf("Processing imports at VA: 0x%x, Size: 0x%x\n", 
           importDir->VirtualAddress, importDir->Size);

    // 验证导入目录地址
    if (importDir->VirtualAddress >= ntHeaders->OptionalHeader.SizeOfImage) {
        printf("Import directory VA is out of bounds\n");
        return 1;
    }

    IMAGE_IMPORT_DESCRIPTOR* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)((char*)imageBase + importDir->VirtualAddress);
    if (!importDesc) {
        printf("Invalid import descriptor\n");
        return 1;
    }

    // 遍历所有导入 DLL
    int dllCount = 0;
    while (importDesc->Name) {
        // 验证DLL名称地址
        if (importDesc->Name >= ntHeaders->OptionalHeader.SizeOfImage) {
            printf("DLL name address is out of bounds\n");
            return 1;
        }

        char* dllName = (char*)imageBase + importDesc->Name;
        printf("  Loading DLL: %s\n", dllName);
        printf("    OriginalFirstThunk: 0x%x\n", importDesc->OriginalFirstThunk);
        printf("    FirstThunk: 0x%x\n", importDesc->FirstThunk);

        // 验证FirstThunk和OriginalFirstThunk
        if (importDesc->FirstThunk >= ntHeaders->OptionalHeader.SizeOfImage ||
            (importDesc->OriginalFirstThunk && 
             importDesc->OriginalFirstThunk >= ntHeaders->OptionalHeader.SizeOfImage)) {
            printf("    Invalid thunk addresses\n");
            return 1;
        }

        int64_t dllBase = (int64_t)LoadLibraryA(dllName);
        if (!dllBase) {
            printf("    Failed to load DLL: %s (Error: %d)\n", dllName, GetLastError());
            return 1;
        }

        // 处理 IAT
        IMAGE_THUNK_DATA64* firstThunk = (IMAGE_THUNK_DATA64*)((char*)imageBase + importDesc->FirstThunk);
        IMAGE_THUNK_DATA64* origThunk = importDesc->OriginalFirstThunk ?
            (IMAGE_THUNK_DATA64*)((char*)imageBase + importDesc->OriginalFirstThunk) : firstThunk;

        int funcCount = 0;
        while (origThunk->u1.AddressOfData) {
            void* funcAddr = NULL;
            char* funcName = NULL;

            if (origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) {
                // 按序号导入
                WORD ordinal = IMAGE_ORDINAL64(origThunk->u1.Ordinal);
                funcAddr = GetProcAddress(dllBase, (const char*)(uintptr_t)ordinal);
                printf("    Import by ordinal: %d -> %p\n", ordinal, funcAddr);
            } else {
                // 验证函数名称地址
                if (origThunk->u1.AddressOfData >= ntHeaders->OptionalHeader.SizeOfImage) {
                    printf("    Function name address is out of bounds\n");
                    return 1;
                }

                // 按名称导入
                IMAGE_IMPORT_BY_NAME* importByName = (IMAGE_IMPORT_BY_NAME*)((char*)imageBase + 
                    origThunk->u1.AddressOfData);
                funcName = (char*)importByName->Name;
                funcAddr = GetProcAddress(dllBase, funcName);
                printf("    Import by name: %s -> %p\n", funcName, funcAddr);
            }

            if (!funcAddr) {
                printf("    Failed to get function address%s%s (Error: %d)\n", 
                    funcName ? ": " : "", 
                    funcName ? funcName : "",
                    GetLastError());
                return 1;
            }

            firstThunk->u1.Function = (ULONGLONG)(uintptr_t)funcAddr;
            origThunk++;
            firstThunk++;

            if (++funcCount > 1000) {  // 防止无限循环
                printf("    Too many functions in one DLL\n");
                return 1;
            }
        }

        importDesc++;
        if (++dllCount > 100) {  // 防止无限循环
            printf("Too many DLLs\n");
            return 1;
        }
    }

    return 0;
}

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

    printf("Processing relocations (Delta: 0x%llx)...\n", delta);

    // 找到包含重定位数据的节
    IMAGE_SECTION_HEADER* sections = (IMAGE_SECTION_HEADER*)((char*)ntHeaders + 
        sizeof(IMAGE_NT_HEADERS64));

    IMAGE_SECTION_HEADER* relocSection = NULL;
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        if (sections[i].VirtualAddress == relocDir->VirtualAddress) {
            relocSection = &sections[i];
            break;
        }
    }

    if (!relocSection) {
        printf("Failed to find relocation section\n");
        return 1;
    }

    printf("Found relocation section at VA: 0x%x, Size: 0x%x\n",
           relocSection->VirtualAddress, relocSection->VirtualSize);

    // 获取重定位数据
    char* relocBase = (char*)imageBase + relocDir->VirtualAddress;
    char* relocEnd = relocBase + relocSection->VirtualSize;

    // 处理每个重定位块
    IMAGE_BASE_RELOCATION* reloc = (IMAGE_BASE_RELOCATION*)relocBase;
    while ((char*)reloc < relocEnd && reloc->VirtualAddress != 0) {
        // 验证块大小
        if (reloc->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION) || 
            (char*)reloc + reloc->SizeOfBlock > relocEnd ||
            reloc->SizeOfBlock > 0x1000) {  // 通常不会超过一个页面大小
            printf("  Invalid block size at VA: 0x%x, Size: 0x%x\n", 
                   reloc->VirtualAddress, reloc->SizeOfBlock);
            break;
        }

        // 验证虚拟地址
        if (reloc->VirtualAddress >= ntHeaders->OptionalHeader.SizeOfImage) {
            printf("  Invalid block VA: 0x%x\n", reloc->VirtualAddress);
            break;
        }

        // 获取重定位项
        WORD* relocData = (WORD*)((char*)reloc + sizeof(IMAGE_BASE_RELOCATION));
        int numEntries = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);

        // 验证条目数量
        if (numEntries <= 0 || numEntries > 1000) {  // 使用一个合理的上限
            printf("  Invalid number of entries: %d\n", numEntries);
            break;
        }

        printf("  Block VA: 0x%x, Entries: %d\n", reloc->VirtualAddress, numEntries);

        // 处理每个重定位项
        for (int i = 0; i < numEntries; i++) {
            WORD entry = relocData[i];
            WORD type = (entry >> 12) & 0xf;
            WORD offset = entry & 0xfff;

            // 验证偏移量
            if (offset >= 0x1000) {  // 页面大小
                printf("    Skip invalid offset: 0x%x\n", offset);
                continue;
            }

            // 计算需要修改的地址
            char* pageRva = (char*)imageBase + reloc->VirtualAddress;
            char* targetAddr = pageRva + offset;

            // 验证目标地址
            if (targetAddr < (char*)imageBase || 
                targetAddr >= (char*)imageBase + ntHeaders->OptionalHeader.SizeOfImage - sizeof(uint64_t)) {
                printf("    Skip invalid target address: %p\n", targetAddr);
                continue;
            }

            // 根据类型进行重定位
            if (type == IMAGE_REL_BASED_DIR64) {
                uint64_t* patchAddr = (uint64_t*)targetAddr;
                uint64_t oldValue = *patchAddr;
                *patchAddr += delta;
                printf("    Relocation at %p: 0x%llx -> 0x%llx\n", 
                       patchAddr, oldValue, *patchAddr);
            }
        }

        // 移动到下一个块
        reloc = (IMAGE_BASE_RELOCATION*)((char*)reloc + reloc->SizeOfBlock);
    }

    return 0;
}

// 错误处理函数
void print_error(const char* msg) {
    printf("Error: %s\n", msg);
}

// PE加载器上下文
typedef struct {
    void* base;           // 映射基址
    size_t size;         // 映射大小
    void* entry;         // 入口点
    BOOL is_dll;         // 是否是DLL
} PE_CONTEXT;

// 初始化PE上下文
PE_CONTEXT* init_pe_context() {
    PE_CONTEXT* ctx = (PE_CONTEXT*)malloc(sizeof(PE_CONTEXT));
    if (!ctx) {
        print_error("Failed to allocate PE context");
        return NULL;
    }
    memset(ctx, 0, sizeof(PE_CONTEXT));
    return ctx;
}

// 清理PE上下文
void cleanup_pe_context(PE_CONTEXT* ctx) {
    if (ctx) {
        if (ctx->base) {
            munmap(ctx->base, ctx->size);
        }
        free(ctx);
    }
}

// 加载PE文件
PE_CONTEXT* load_pe_file(const char* path) {
    PE_CONTEXT* ctx = init_pe_context();
    if (!ctx) return NULL;

    printf("\nLoading PE file: %s\n", path);

    // 打开文件
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open file: %s (errno: %d)\n", path, errno);
        cleanup_pe_context(ctx);
        return NULL;
    }

    // 获取文件大小
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Failed to get file size (errno: %d)\n", errno);
        close(fd);
        cleanup_pe_context(ctx);
        return NULL;
    }

    printf("File size: %lld bytes\n", (long long)st.st_size);

    // 验证文件大小
    if (st.st_size < sizeof(IMAGE_DOS_HEADER)) {
        print_error("File too small");
        close(fd);
        cleanup_pe_context(ctx);
        return NULL;
    }

    // 读取文件内容
    void* file_data = malloc(st.st_size);
    if (!file_data) {
        print_error("Failed to allocate memory for file");
        close(fd);
        cleanup_pe_context(ctx);
        return NULL;
    }

    ssize_t bytes_read = read(fd, file_data, st.st_size);
    if (bytes_read != st.st_size) {
        printf("Failed to read file: expected %lld bytes, got %lld (errno: %d)\n",
               (long long)st.st_size, (long long)bytes_read, errno);
        free(file_data);
        close(fd);
        cleanup_pe_context(ctx);
        return NULL;
    }

    close(fd);

    // 解析DOS头
    IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)file_data;
    if (dos_header->e_magic != 0x5A4D) {
        printf("Invalid DOS signature: 0x%x\n", dos_header->e_magic);
        free(file_data);
        cleanup_pe_context(ctx);
        return NULL;
    }

    printf("DOS Header:\n");
    printf("  Magic: 0x%x\n", dos_header->e_magic);
    printf("  NT Headers offset: 0x%x\n", dos_header->e_lfanew);

    // 验证NT头偏移
    if (dos_header->e_lfanew < sizeof(IMAGE_DOS_HEADER) || 
        dos_header->e_lfanew >= st.st_size - sizeof(IMAGE_NT_HEADERS64)) {
        printf("Invalid NT headers offset: 0x%x (file size: 0x%llx)\n",
               dos_header->e_lfanew, (unsigned long long)st.st_size);
        free(file_data);
        cleanup_pe_context(ctx);
        return NULL;
    }

    // 解析NT头
    IMAGE_NT_HEADERS64* nt_headers = (IMAGE_NT_HEADERS64*)((char*)file_data + dos_header->e_lfanew);
    if (nt_headers->Signature != 0x4550) {
        print_error("Invalid PE signature");
        free(file_data);
        cleanup_pe_context(ctx);
        return NULL;
    }

    printf("\nPE File Analysis:\n");
    printf("  Machine: 0x%x\n", nt_headers->FileHeader.Machine);
    printf("  Characteristics: 0x%x\n", nt_headers->FileHeader.Characteristics);
    printf("  Subsystem: 0x%x\n", nt_headers->OptionalHeader.Subsystem);
    printf("  DllCharacteristics: 0x%x\n", nt_headers->OptionalHeader.DllCharacteristics);
    
    // Print Data Directories
    printf("\nData Directories:\n");
    for (int i = 0; i < 16; i++) {
        if (nt_headers->OptionalHeader.DataDirectory[i].VirtualAddress) {
            printf("  [%2d] VA: 0x%x, Size: 0x%x\n", i,
                   nt_headers->OptionalHeader.DataDirectory[i].VirtualAddress,
                   nt_headers->OptionalHeader.DataDirectory[i].Size);
        }
    }

    printf("\nPE file info:\n");
    printf("  ImageBase: 0x%llx\n", nt_headers->OptionalHeader.ImageBase);
    printf("  SizeOfImage: 0x%x\n", nt_headers->OptionalHeader.SizeOfImage);
    printf("  NumberOfSections: %d\n", nt_headers->FileHeader.NumberOfSections);

    // 验证节表
    size_t sectionTableOffset = dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS64);
    if (sectionTableOffset >= st.st_size || 
        sectionTableOffset + nt_headers->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER) > st.st_size) {
        print_error("Invalid section table");
        free(file_data);
        cleanup_pe_context(ctx);
        return NULL;
    }

    // 分配内存
    size_t image_size = nt_headers->OptionalHeader.SizeOfImage;
    ctx->size = (image_size + 0xFFF) & ~0xFFF;
    ctx->base = mmap(NULL, ctx->size,
                    PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS,
                    -1, 0);

    if (ctx->base == MAP_FAILED) {
        print_error("Failed to allocate memory for image");
        free(file_data);
        cleanup_pe_context(ctx);
        return NULL;
    }

    printf("Memory allocated at %p\n", ctx->base);

    // 复制PE头
    size_t headers_size = nt_headers->OptionalHeader.SizeOfHeaders;
    if (headers_size > st.st_size) {
        print_error("Invalid headers size");
        free(file_data);
        cleanup_pe_context(ctx);
        return NULL;
    }
    memcpy(ctx->base, file_data, headers_size);

    // 复制节
    IMAGE_SECTION_HEADER* sections = (IMAGE_SECTION_HEADER*)((char*)nt_headers + sizeof(IMAGE_NT_HEADERS64));
    for (int i = 0; i < nt_headers->FileHeader.NumberOfSections; i++) {
        // 验证节的地址和大小
        if (sections[i].VirtualAddress >= ctx->size ||
            sections[i].VirtualAddress + sections[i].VirtualSize > ctx->size ||
            sections[i].PointerToRawData >= st.st_size ||
            sections[i].PointerToRawData + sections[i].SizeOfRawData > st.st_size) {
            printf("Invalid section %d\n", i);
            continue;
        }

        void* dest = (char*)ctx->base + sections[i].VirtualAddress;
        void* src = (char*)file_data + sections[i].PointerToRawData;
        
        printf("Section %d: %.*s\n", i, 8, sections[i].Name);
        printf("  VA: 0x%x, Size: 0x%x\n", sections[i].VirtualAddress, sections[i].VirtualSize);
        
        // 清零整个节
        size_t virtual_size = (sections[i].VirtualSize + 0xFFF) & ~0xFFF;
        memset(dest, 0, virtual_size);

        // 复制原始数据
        if (sections[i].SizeOfRawData > 0) {
            memcpy(dest, src, sections[i].SizeOfRawData);
        }
    }

    // 计算重定位增量
    uint64_t delta = (uint64_t)ctx->base - nt_headers->OptionalHeader.ImageBase;
    printf("Relocation delta: 0x%llx\n", delta);

    // 处理重定位
    if (process_relocations(ctx->base, (IMAGE_NT_HEADERS64*)((char*)ctx->base + dos_header->e_lfanew), delta) != 0) {
        print_error("Failed to process relocations");
        free(file_data);
        cleanup_pe_context(ctx);
        return NULL;
    }

    // 处理导入表
    if (process_imports(ctx->base, (IMAGE_NT_HEADERS64*)((char*)ctx->base + dos_header->e_lfanew)) != 0) {
        print_error("Failed to process imports");
        free(file_data);
        cleanup_pe_context(ctx);
        return NULL;
    }

    // 设置最终的页面权限
    for (int i = 0; i < nt_headers->FileHeader.NumberOfSections; i++) {
        void* dest = (char*)ctx->base + sections[i].VirtualAddress;
        size_t virtual_size = (sections[i].VirtualSize + 0xFFF) & ~0xFFF;

        int prot = PROT_READ;
        if (sections[i].Characteristics & IMAGE_SCN_MEM_WRITE)   prot |= PROT_WRITE;
        if (sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) prot |= PROT_EXEC;

        if (mprotect(dest, virtual_size, prot) != 0) {
            printf("Warning: Failed to set section %d permissions\n", i);
        }
    }

    // 设置入口点
    ctx->entry = (char*)ctx->base + nt_headers->OptionalHeader.AddressOfEntryPoint;
    ctx->is_dll = (nt_headers->FileHeader.Characteristics & 0x2000) != 0;

    printf("Entry point at %p\n", ctx->entry);

    free(file_data);
    return ctx;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <target>\n", argv[0]);
        return 1;
    }

    // 加载PE文件
    PE_CONTEXT* ctx = load_pe_file(argv[1]);
    if (!ctx) {
        return 1;
    }

    printf("PE file loaded at %p, entry point at %p\n", ctx->base, ctx->entry);

    // 执行入口点
    typedef int (*DllMain)(void* hinstDLL, unsigned long fdwReason, void* lpvReserved);
    DllMain ep = (DllMain)ctx->entry;
    
    printf("Executing...\n");
    int result = ep(ctx->base, 1, NULL);  // 1 = DLL_PROCESS_ATTACH
    printf("Execution result: %d\n", result);

    // 清理
    cleanup_pe_context(ctx);
    return result;
}
