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

//-----------------------------------------------------------------------------
// 辅助函数
//-----------------------------------------------------------------------------

// 以十六进制格式打印内存区域的内容
static inline void hex_dump(const char* prefix, const void* data, size_t size) {
    const unsigned char* p = data;
    for (size_t i = 0; i < size; i += 16) {
        printf("%s%04zx:", prefix, i);
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                printf(" %02x", p[i + j]);
            } else {
                printf("   ");
            }
        }
        printf("\n");
    }
}

// 以小端序读取32位整数，并打印调试信息
static inline uint32_t READ32(const unsigned char* p) {
    printf("Reading 32-bit value at offset %p: %02x %02x %02x %02x = ", 
           p, p[0], p[1], p[2], p[3]);
    uint32_t val = ((uint32_t)p[0]) |
                   ((uint32_t)p[1] << 8) |
                   ((uint32_t)p[2] << 16) |
                   ((uint32_t)p[3] << 24);
    printf("%u (0x%x)\n", val, val);
    return val;
}

// 解析printf语句中的八进制转义序列
static inline int parse_octal(const unsigned char *data, int i, int *pc) {
    int c;
    if ('0' <= data[i] && data[i] <= '7') {
        c = data[i++] - '0';
        if ('0' <= data[i] && data[i] <= '7') {
            c *= 8;
            c += data[i++] - '0';
            if ('0' <= data[i] && data[i] <= '7') {
                c *= 8;
                c += data[i++] - '0';
            }
        }
        *pc = c;
    }
    return i;
}

//-----------------------------------------------------------------------------
// ELF头验证
//-----------------------------------------------------------------------------

// 验证ELF头的各个字段是否有效
static inline int validate_elf_header(const unsigned char *elf_buf, size_t elf_len) {
    if (elf_len < sizeof(Elf64_Ehdr)) {
        printf("ELF header too small\n");
        return 0;
    }
    
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr*)elf_buf;
    
    // 验证魔数
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        printf("Invalid ELF magic\n");
        return 0;
    }
    
    // 验证是否为64位ELF
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        printf("Not a 64-bit ELF\n");
        return 0;
    }
    
    // 验证字节序
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        printf("Not little-endian\n");
        return 0;
    }
    
    // 验证版本
    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
        printf("Invalid ELF version\n");
        return 0;
    }
    
    // 验证文件类型
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        printf("Not an executable or shared object\n");
        return 0;
    }
    
    // 验证目标架构
    if (ehdr->e_machine != EM_X86_64) {
        printf("Not x86_64 architecture\n");
        return 0;
    }
    
    // 打印ELF头的详细信息
    printf("\nELF header details:\n");
    printf("  Entry point: 0x%lx\n", ehdr->e_entry);
    printf("  Program header offset: 0x%lx\n", ehdr->e_phoff);
    printf("  Section header offset: 0x%lx\n", ehdr->e_shoff);
    printf("  Flags: 0x%x\n", ehdr->e_flags);
    printf("  Header size: %d\n", ehdr->e_ehsize);
    printf("  Program header size: %d\n", ehdr->e_phentsize);
    printf("  Program header count: %d\n", ehdr->e_phnum);
    printf("  Section header size: %d\n", ehdr->e_shentsize);
    printf("  Section header count: %d\n", ehdr->e_shnum);
    printf("  Section name string table index: %d\n", ehdr->e_shstrndx);
    
    return 1;
}

//-----------------------------------------------------------------------------
// ELF头搜索
//-----------------------------------------------------------------------------

// 在printf语句中搜索ELF头
static inline int search_elf_header(const unsigned char *data, size_t size) {
    printf("\nSearching for ELF header in printf statements...\n");
    
    // 只搜索前8192字节
    size_t search_size = size < 8192 ? size : 8192;
    
    // 查找包含八进制转义序列的printf语句
    for (size_t i = 0; i < search_size - 16; i++) {
        // 检查printf语句的开始（支持单引号和双引号）
        if ((data[i] == 'p' && data[i+1] == 'r' && data[i+2] == 'i' && 
             data[i+3] == 'n' && data[i+4] == 't' && data[i+5] == 'f' &&
             (data[i+6] == ' ' || data[i+6] == '\t') &&
             (data[i+7] == '\'' || data[i+7] == '"')) ||
            (data[i] == 'p' && data[i+1] == 'r' && data[i+2] == 'i' && 
             data[i+3] == 'n' && data[i+4] == 't' && data[i+5] == 'f' &&
             data[i+6] == '\\' && data[i+7] == '\'')) {
            
            printf("Found printf at offset 0x%zx\n", i);
            
            // 跳过到引号
            i += 7;
            while (i < search_size && data[i] != '\'' && data[i] != '"') i++;
            if (i >= search_size) continue;
            i++;
            
            // 解析八进制转义序列
            unsigned char elf_buf[64];
            int elf_len = 0;
            
            while (i < search_size && elf_len < sizeof(elf_buf)) {
                if (data[i] == '\\') {
                    i++;
                    if (i >= search_size) break;
                    
                    // 解析八进制转义序列
                    int c;
                    i = parse_octal(data, i, &c);
                    if (c >= 0) {
                        elf_buf[elf_len++] = c;
                    }
                }
                else if (data[i] == '\'' || data[i] == '"') {
                    break;
                }
                else if (data[i] >= 32 && data[i] <= 126) {
                    // 直接复制可打印ASCII字符
                    elf_buf[elf_len++] = data[i++];
                }
                else {
                    i++;
                }
            }
            
            // 检查是否找到ELF头
            if (elf_len >= 16 &&
                elf_buf[0] == 0x7f && elf_buf[1] == 'E' && 
                elf_buf[2] == 'L' && elf_buf[3] == 'F') {
                
                printf("Found ELF header in printf statement:\n");
                printf("  Magic: %02x %02x %02x %02x\n", 
                       elf_buf[0], elf_buf[1], elf_buf[2], elf_buf[3]);
                printf("  Class: %d\n", elf_buf[4]);
                printf("  Data: %d\n", elf_buf[5]); 
                printf("  Type: %d\n", elf_buf[16]);
                printf("  Machine: %d\n", elf_buf[18]);
                
                // 验证完整的ELF头
                if (validate_elf_header(elf_buf, elf_len)) {
                    printf("ELF header validation passed\n");
                    return 1;
                }
                printf("ELF header validation failed\n");
            }
        }
    }
    
    // 如果在printf语句中没有找到ELF头
    printf("No ELF header found in printf statements\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <target>\n", argv[0]);
        return 1;
    }
    
    printf("test_loader starting...\n");
    printf("Loading target: %s\n", argv[1]);
    
    // Open target file
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("Failed to open target file\n");
        return 1;
    }
    
    // Get file size
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Failed to get file size\n");
        close(fd);
        return 1;
    }
    printf("File size: %zu bytes\n", st.st_size);
    
    // Map file into memory
    void *mapped_data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped_data == MAP_FAILED) {
        printf("Failed to map file\n");
        close(fd);
        return 1;
    }
    printf("Mapped at address: %p\n\n", mapped_data);
    
    // Analyze APE header
    const unsigned char *raw = mapped_data;
    printf("Analyzing APE header...\n");
    hex_dump("  ", raw, 64);
    
    // Read and validate APE header
    uint32_t size = READ32(raw + 8);
    uint32_t elf_off = READ32(raw + 12);
    
    printf("APE header validation:\n");
    printf("  File size: 0x%x\n", (uint32_t)st.st_size);
    printf("  APE size: 0x%x\n", size);
    printf("  ELF offset: 0x%x\n", elf_off);
    
    // Print raw bytes for debugging
    printf("Raw bytes for size: %02x %02x %02x %02x\n",
           raw[8], raw[9], raw[10], raw[11]);
    printf("Raw bytes for elf_off: %02x %02x %02x %02x\n",
           raw[12], raw[13], raw[14], raw[15]);
    
    // Search for ELF header
    if (!search_elf_header(raw, st.st_size)) {
        printf("Failed to locate valid ELF header\n");
        munmap(mapped_data, st.st_size);
        close(fd);
        return 1;
    }
    
    // Cleanup
    munmap(mapped_data, st.st_size);
    close(fd);
    
    return 0;
}

