#include "ape_loader.h"

/* 平台特定的加载函数实现 */
static void* platform_load(const char* path) {
    printf("Opening file: %s\n", path);

    /* 打开文件 */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open file: %d\n", errno);
        return NULL;
    }

    /* 获取文件大小 */
    off_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0) {
        printf("Failed to get file size: %d\n", errno);
        close(fd);
        return NULL;
    }
    printf("File size: %lld\n", size);
    lseek(fd, 0, SEEK_SET);

    /* 映射文件到内存 */
    void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE, fd, 0);
    close(fd);

    if (addr == MAP_FAILED) {
        printf("Failed to map file: %d\n", errno);
        return NULL;
    }

    printf("File mapped at: %p\n", addr);
    return addr;
}

static void* platform_get_proc(void* handle, const char* symbol) {
    /* 简单地返回入口点 */
    struct ApeHeader* header = (struct ApeHeader*)handle;
    void* entry = (void*)((char*)handle + header->elf_entry);
    printf("Entry point: %p\n", entry);
    return entry;
}

static int platform_unload(void* handle) {
    /* 获取文件大小 */
    struct stat st;
    if (fstat(fileno(handle), &st) < 0) {
        printf("Failed to get file size for unmap: %d\n", errno);
        return -1;
    }

    /* 解除映射 */
    int result = munmap(handle, st.st_size);
    if (result != 0) {
        printf("Failed to unmap file: %d\n", errno);
    }
    return result;
}

/* APE 加载器实现 */
void* ape_load(const char* path) {
    void* handle = platform_load(path);
    if (!handle) {
        return NULL;
    }
    
    /* 验证APE头部 */
    struct ApeHeader* header = (struct ApeHeader*)handle;
    printf("Verifying APE header:\n");
    printf("  MZ magic: 0x%llx\n", header->mz_magic);
    printf("  PE magic: 0x%x\n", header->pe_magic);
    printf("  ELF magic: 0x%x\n", header->elf_magic);
    printf("  Mach-O magic: 0x%x\n", header->macho_magic);

    if (header->mz_magic != 0x5A4D || 
        header->pe_magic != 0x4550 ||
        header->elf_magic != 0x464C457F ||
        header->macho_magic != 0xFEEDFACF) {
        printf("Invalid APE header\n");
        platform_unload(handle);
        return NULL;
    }
    
    printf("APE header verified\n");
    return handle;
}

void* ape_get_proc(void* handle, const char* symbol) {
    return platform_get_proc(handle, symbol);
}

int ape_unload(void* handle) {
    return platform_unload(handle);
} 