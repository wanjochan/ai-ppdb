#include "cosmopolitan.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/auxv.h"
#include "libc/elf/elf.h"

// ApeLoader的声明
extern void* ApeLoader(const char* path, long* sp, long* auxv, long pagesz, int os);

#define PAGE_SIZE 4096
#define ROUND_UP(x, y) (((x) + (y) - 1) & -(y))
#define ROUND_DOWN(x, y) ((x) & -(y))
#define APE_MAGIC "MZqFpD="

#pragma pack(push, 1)  // 确保1字节对齐
// APE文件头结构
struct ApeHeader {
    char magic[8];     // "MZqFpD=" + 1字节填充
    uint32_t size;     // 文件大小
    uint32_t elf_off;  // ELF偏移
    uint8_t reserved[48];  // 保留字节
};
#pragma pack(pop)

// auxv常量定义（如果头文件中没有）
#ifndef AT_NULL
#define AT_NULL         0
#define AT_IGNORE      1
#define AT_EXECFD      2
#define AT_PHDR        3
#define AT_PHENT       4
#define AT_PHNUM       5
#define AT_PAGESZ      6
#define AT_BASE        7
#define AT_FLAGS       8
#define AT_ENTRY       9
#endif

// 辅助函数：从字节数组中读取64位整数（小端序）
static uint64_t READ64(const unsigned char* p) {
    return ((uint64_t)p[0]) |
           ((uint64_t)p[1] << 8) |
           ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

// 辅助函数：从字节数组中读取32位整数（小端序）
static uint32_t READ32(const unsigned char* p) {
    uint32_t val = ((uint32_t)p[0]) |
                   ((uint32_t)p[1] << 8) |
                   ((uint32_t)p[2] << 16) |
                   ((uint32_t)p[3] << 24);
    printf("Reading 32-bit value at offset %p: %02x %02x %02x %02x = %u (0x%x)\n",
           p, p[0], p[1], p[2], p[3], val, val);
    return val;
}

// 辅助函数：打印内存区域的十六进制转储
static void hex_dump(const char* prefix, const unsigned char* data, size_t size) {
    for(size_t i = 0; i < size; i++) {
        if(i % 16 == 0) printf("%s%04zx:", prefix, i);
        printf(" %02x", data[i]);
        if(i % 16 == 15) printf("\n");
    }
    if(size % 16 != 0) printf("\n");
}

// 辅助函数：验证ELF头
static int validate_elf_header(const Elf64_Ehdr* ehdr, const char* path) {
    if (memcmp(ehdr->e_ident, "\x7f""ELF", 4) != 0) {
        printf("Invalid ELF magic\n");
        return 0;
    }
    
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        printf("Not a 64-bit ELF\n");
        return 0;
    }
    
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        printf("Not a little-endian ELF\n");
        return 0;
    }
    
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        printf("Not an executable or shared object\n");
        return 0;
    }
    
    if (ehdr->e_machine != EM_X86_64) {
        printf("Not an x86_64 ELF\n");
        return 0;
    }
    
    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
        printf("No program headers\n");
        return 0;
    }
    
    return 1;
}

// 辅助函数：验证APE头
static int validate_ape_header(const struct ApeHeader* ape, size_t file_size) {
    // 验证魔数
    if (memcmp(ape->magic, APE_MAGIC, strlen(APE_MAGIC)) != 0) {
        printf("Invalid APE magic\n");
        return 0;
    }
    
    // 读取并验证大小和偏移
    const unsigned char* raw = (const unsigned char*)ape;
    
    // 使用READ32读取size和elf_off
    uint32_t size = READ32(raw + 8);
    uint32_t elf_off = READ32(raw + 12);
    
    printf("APE header validation:\n");
    printf("  File size: 0x%zx\n", file_size);
    printf("  APE size: 0x%x\n", size);
    printf("  ELF offset: 0x%x\n", elf_off);
    
    // 打印原始字节以进行调试
    printf("Raw bytes for size: %02x %02x %02x %02x\n", 
           raw[8], raw[9], raw[10], raw[11]);
    printf("Raw bytes for elf_off: %02x %02x %02x %02x\n", 
           raw[12], raw[13], raw[14], raw[15]);
    
    // 验证ELF偏移
    if (elf_off > file_size - sizeof(Elf64_Ehdr)) {
        printf("Invalid ELF offset (outside file bounds)\n");
        return 0;
    }
    
    return 1;
}

// 辅助函数：读取程序头表
static int read_program_headers(Elf64_Phdr* phdr, const unsigned char* base,
                              const Elf64_Ehdr* ehdr, uint32_t elf_off,
                              size_t file_size) {
    // 计算程序头表的位置
    const unsigned char* phdr_ptr = base + elf_off + ehdr->e_phoff;
    size_t total_size = ehdr->e_phnum * sizeof(Elf64_Phdr);
    
    // 验证程序头表的范围
    if (phdr_ptr + total_size > base + file_size) {
        printf("Program header table extends beyond file end\n");
        return 0;
    }
    
    // 读取每个程序头
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr* curr = &phdr[i];
        const unsigned char* curr_ptr = phdr_ptr + i * sizeof(Elf64_Phdr);
        
        curr->p_type = READ32(curr_ptr);
        curr->p_flags = READ32(curr_ptr + 4);
        curr->p_offset = READ64(curr_ptr + 8);
        curr->p_vaddr = READ64(curr_ptr + 16);
        curr->p_paddr = READ64(curr_ptr + 24);
        curr->p_filesz = READ64(curr_ptr + 32);
        curr->p_memsz = READ64(curr_ptr + 40);
        curr->p_align = READ64(curr_ptr + 48);
        
        // 验证段的范围
        if (curr->p_offset + curr->p_filesz > file_size) {
            printf("Segment %d extends beyond file end\n", i);
            return 0;
        }
        
        printf("Program header %d:\n", i);
        printf("  Type: 0x%x\n", curr->p_type);
        printf("  Flags: 0x%x\n", curr->p_flags);
        printf("  Offset: 0x%lx\n", curr->p_offset);
        printf("  VAddr: 0x%lx\n", curr->p_vaddr);
        printf("  PAddr: 0x%lx\n", curr->p_paddr);
        printf("  FileSize: 0x%lx\n", curr->p_filesz);
        printf("  MemSize: 0x%lx\n", curr->p_memsz);
        printf("  Align: 0x%lx\n", curr->p_align);
        printf("  Raw data:\n");
        hex_dump("    ", curr_ptr, sizeof(Elf64_Phdr));
    }
    
    return 1;
}

int main(int argc, char* argv[]) {
    printf("test_loader starting...\n");
    
    if (argc != 2) {
        printf("Usage: %s <target>\n", argv[0]);
        return 1;
    }
    
    const char* target = argv[1];
    printf("Loading target: %s\n", target);
    
    // 打开目标文件
    int fd = open(target, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open target file\n");
        return 1;
    }
    
    // 获取文件大小
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Failed to get file size\n");
        close(fd);
        return 1;
    }
    size_t file_size = st.st_size;
    printf("File size: %zu bytes\n", file_size);
    
    // 映射文件到内存
    void* mapped = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        printf("Failed to map file\n");
        close(fd);
        return 1;
    }
    printf("Mapped at address: %p\n\n", mapped);
    
    // 分析APE头
    printf("Analyzing APE header...\n");
    const struct ApeHeader* ape = (const struct ApeHeader*)mapped;
    
    // 打印APE头的前64字节
    const unsigned char* raw = (const unsigned char*)ape;
    for (int i = 0; i < 64; i += 16) {
        printf("  %04x:", i);
        for (int j = 0; j < 16; j++) {
            printf(" %02x", raw[i + j]);
        }
        printf("\n");
    }
    
    // 验证APE头
    if (!validate_ape_header(ape, file_size)) {
        munmap(mapped, file_size);
        close(fd);
        return 1;
    }
    
    // 继续处理...
    
    munmap(mapped, file_size);
    close(fd);
    return 0;
}
