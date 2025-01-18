#include "cosmopolitan.h"
#include "ape/ape.h"
#include "libc/elf/elf.h"
#include "libc/runtime/runtime.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/prot.h"

// auxv常量定义
#define AT_NULL   0
#define AT_IGNORE 1
#define AT_EXECFD 2
#define AT_PHDR   3
#define AT_PHENT  4
#define AT_PHNUM  5
#define AT_PAGESZ 6
#define AT_BASE   7
#define AT_FLAGS  8
#define AT_ENTRY  9
#define AT_NOTELF 10
#define AT_UID    11
#define AT_EUID   12
#define AT_GID    13
#define AT_EGID   14

// APE文件格式常量
#define APE_MAGIC "MZqFpD"
#define APE_HEADER_SIZE 4096

// ApeLoader的声明
extern __attribute__((__noreturn__)) void ApeLoader(long di, long *sp, char dl);

// 将八进制字符串转换为字节
static int oct_to_byte(const char* str) {
    int result = 0;
    for (int i = 0; i < 3 && str[i] >= '0' && str[i] <= '7'; i++) {
        result = result * 8 + (str[i] - '0');
    }
    return result;
}

// 将字符串形式的ELF头转换为二进制
static void* convert_elf_str_to_bin(const char* str, void* dest) {
    unsigned char* out = (unsigned char*)dest;
    const char* p = str;
    int pos = 0;

    while (*p) {
        if (*p == '\\') {
            p++;
            if (*p >= '0' && *p <= '7') {
                // 八进制数字
                out[pos++] = oct_to_byte(p);
                while (*p >= '0' && *p <= '7') p++;
            } else {
                // 转义字符
                switch (*p) {
                    case 'n': out[pos++] = '\n'; break;
                    case 'r': out[pos++] = '\r'; break;
                    case 't': out[pos++] = '\t'; break;
                    default: out[pos++] = *p; break;
                }
                p++;
            }
        } else {
            out[pos++] = *p++;
        }
    }
    return dest;
}

// 查找ELF头部
static void* find_elf_header(void* base, size_t size) {
    // 检查APE魔数
    if (memcmp(base, APE_MAGIC, 6) != 0) {
        printf("Invalid APE magic\n");
        return NULL;
    }

    // 打印前64字节的内容
    printf("First 64 bytes:\n");
    unsigned char* p = (unsigned char*)base;
    for (int i = 0; i < 64; i++) {
        printf("%02x ", p[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }

    // 搜索字符串形式的ELF头
    const char* elf_str = "\\177ELF\\2\\1\\1";
    char* ptr = base;
    for (size_t i = 0; i < size - 8; i++) {
        if (strstr(ptr + i, elf_str) != NULL) {
            printf("Found ELF header string at offset 0x%zx\n", i);
            
            // 分配临时缓冲区
            unsigned char temp[sizeof(Elf64_Ehdr)];
            memset(temp, 0, sizeof(temp));
            
            // 获取完整的字符串
            char str_buf[1024];
            size_t str_len = 0;
            const char* src = ptr + i;
            while (str_len < sizeof(str_buf) - 1 && *src && *src != '\'') {
                str_buf[str_len++] = *src++;
            }
            str_buf[str_len] = '\0';
            
            printf("ELF header string: %s\n", str_buf);
            
            // 转换为二进制
            convert_elf_str_to_bin(str_buf, temp);
            
            // 验证转换后的ELF头
            Elf64_Ehdr* ehdr = (Elf64_Ehdr*)temp;
            printf("  Converted ELF header details:\n");
            printf("    Magic: %02x %02x %02x %02x\n", 
                   ehdr->e_ident[0], ehdr->e_ident[1], 
                   ehdr->e_ident[2], ehdr->e_ident[3]);
            printf("    Class: %d (expected 2)\n", ehdr->e_ident[EI_CLASS]);
            printf("    Data: %d (expected 1)\n", ehdr->e_ident[EI_DATA]);
            printf("    Version: %d (expected 1)\n", ehdr->e_ident[EI_VERSION]);
            printf("    Type: %d (expected 3)\n", ehdr->e_type);
            printf("    Machine: %d (expected 62)\n", ehdr->e_machine);

            if (ehdr->e_ident[0] == 0x7f && 
                memcmp(ehdr->e_ident + 1, "ELF", 3) == 0 &&
                ehdr->e_ident[EI_CLASS] == ELFCLASS64 &&
                ehdr->e_ident[EI_DATA] == ELFDATA2LSB &&
                ehdr->e_ident[EI_VERSION] == EV_CURRENT) {
                printf("Found valid ELF header in string form\n");
                // 复制到新的内存区域
                void* new_base = malloc(size);
                if (!new_base) {
                    printf("Failed to allocate memory\n");
                    return NULL;
                }
                memcpy(new_base, temp, sizeof(Elf64_Ehdr));
                return new_base;
            }
            printf("Invalid ELF header in string form\n");
        }
    }

    // 搜索二进制形式的ELF头
    const unsigned char elf_magic[] = {0x7f, 'E', 'L', 'F'};
    for (size_t i = 0; i < size - 4; i++) {
        if (memcmp(p + i, elf_magic, 4) == 0) {
            printf("Found potential ELF header at offset 0x%zx\n", i);
            Elf64_Ehdr* ehdr = (Elf64_Ehdr*)(p + i);
            printf("  ELF header details:\n");
            printf("    Class: %d (expected 2)\n", ehdr->e_ident[EI_CLASS]);
            printf("    Data: %d (expected 1)\n", ehdr->e_ident[EI_DATA]);
            printf("    Version: %d (expected 1)\n", ehdr->e_ident[EI_VERSION]);
            printf("    Type: %d (expected 3)\n", ehdr->e_type);
            printf("    Machine: %d (expected 62)\n", ehdr->e_machine);

            if (ehdr->e_ident[EI_CLASS] == ELFCLASS64 &&
                ehdr->e_ident[EI_DATA] == ELFDATA2LSB &&
                ehdr->e_ident[EI_VERSION] == EV_CURRENT &&
                ehdr->e_type == ET_DYN &&
                ehdr->e_machine == EM_X86_64) {
                printf("Found valid ELF header at offset 0x%zx\n", i);
                return ehdr;
            } else {
                printf("Found invalid ELF header at offset 0x%zx\n", i);
            }
        }
    }

    printf("No ELF header found\n");
    return NULL;
}

int main(int argc, char* argv[]) {
    printf("test_loader starting...\n");
    printf("Arguments: %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d] = %s\n", i, argv[i]);
    }
    
    if (argc < 2) {
        printf("Usage: %s <target_exe> [args...]\n", argv[0]);
        return 1;
    }
    
    printf("Loading target: %s\n", argv[1]);
    ShowCrashReports();  // 启用崩溃报告
    
    // 打开目标文件
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("Failed to open target file: %s\n", argv[1]);
        return 1;
    }
    
    // 获取文件大小
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Failed to stat target file\n");
        close(fd);
        return 1;
    }
    
    // 映射文件到内存
    void* base = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (base == MAP_FAILED) {
        printf("Failed to map target file into memory\n");
        return 1;
    }
    
    // 查找ELF头部
    void* elf_base = find_elf_header(base, st.st_size);
    if (!elf_base) {
        printf("No ELF header found\n");
        munmap(base, st.st_size);
        return 1;
    }
    
    // 检查ELF头部
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)elf_base;
    printf("ELF header info:\n");
    printf("  Type: %d\n", ehdr->e_type);
    printf("  Machine: %d\n", ehdr->e_machine);
    printf("  Version: %d\n", ehdr->e_version);
    printf("  Entry: 0x%lx\n", ehdr->e_entry);
    printf("  PHoff: 0x%lx\n", ehdr->e_phoff);
    printf("  SHoff: 0x%lx\n", ehdr->e_shoff);
    printf("  Flags: 0x%x\n", ehdr->e_flags);
    printf("  EHsize: %d\n", ehdr->e_ehsize);
    printf("  PHentsize: %d\n", ehdr->e_phentsize);
    printf("  PHnum: %d\n", ehdr->e_phnum);
    printf("  SHentsize: %d\n", ehdr->e_shentsize);
    printf("  SHnum: %d\n", ehdr->e_shnum);
    printf("  SHstrndx: %d\n", ehdr->e_shstrndx);
    
    // 获取程序头部信息
    Elf64_Phdr* phdr = (Elf64_Phdr*)((char*)elf_base + ehdr->e_phoff);
    
    // 分配栈空间（16字节对齐）
    size_t stack_size = 32768;  // 32KB栈
    void* stack_mem = memalign(16, stack_size);
    if (!stack_mem) {
        printf("Failed to allocate stack memory\n");
        munmap(base, st.st_size);
        return 1;
    }
    memset(stack_mem, 0, stack_size);
    
    // 设置栈指针（从栈顶开始，向下增长）
    long* sp = (long*)((char*)stack_mem + stack_size - 16);
    sp = (long*)((uintptr_t)sp & ~15ULL);  // 确保16字节对齐
    
    // 计算新的argc（减去loader自身）
    int new_argc = 1;  // 只传递目标程序名
    
    // 设置栈布局
    sp[0] = new_argc;  // 新的argc
    sp[1] = (long)argv[1];  // argv[0] = target_exe
    sp[2] = 0;  // argv结束标记
    sp[3] = 0;  // envp开始（空）
    sp[4] = AT_PHDR;  // auxv[0].a_type
    sp[5] = (long)phdr;  // auxv[0].a_val
    sp[6] = AT_PHENT;  // auxv[1].a_type
    sp[7] = sizeof(Elf64_Phdr);  // auxv[1].a_val
    sp[8] = AT_PHNUM;  // auxv[2].a_type
    sp[9] = ehdr->e_phnum;  // auxv[2].a_val
    sp[10] = AT_PAGESZ;  // auxv[3].a_type
    sp[11] = 4096;  // auxv[3].a_val
    sp[12] = AT_ENTRY;  // auxv[4].a_type
    sp[13] = ehdr->e_entry;  // auxv[4].a_val
    sp[14] = AT_NULL;  // auxv结束标记
    sp[15] = 0;
    
    printf("Stack setup before ApeLoader:\n");
    printf("  new_argc = %d\n", new_argc);
    printf("  sp = %p\n", (void*)sp);
    printf("  stack alignment = %s\n", ((uintptr_t)sp & 15) ? "unaligned" : "aligned");
    printf("  sp[0] (argc) = %ld\n", sp[0]);
    printf("  sp[1] (argv[0]) = %p -> %s\n", (void*)sp[1], (char*)sp[1]);
    printf("  sp[2] = %ld\n", sp[2]);
    printf("  sp[3] = %ld\n", sp[3]);
    printf("  sp[4] = %ld (AT_PHDR)\n", sp[4]);
    printf("  sp[5] = %p\n", (void*)sp[5]);
    printf("  sp[6] = %ld (AT_PHENT)\n", sp[6]);
    printf("  sp[7] = %ld\n", sp[7]);
    printf("  sp[8] = %ld (AT_PHNUM)\n", sp[8]);
    printf("  sp[9] = %ld\n", sp[9]);
    printf("  sp[10] = %ld (AT_PAGESZ)\n", sp[10]);
    printf("  sp[11] = %ld\n", sp[11]);
    printf("  sp[12] = %ld (AT_ENTRY)\n", sp[12]);
    printf("  sp[13] = %lx\n", sp[13]);
    printf("  sp[14] = %ld (AT_NULL)\n", sp[14]);
    printf("  sp[15] = %ld\n", sp[15]);
    
    printf("\nCalling ApeLoader with:\n");
    printf("  di = %d\n", new_argc);
    printf("  sp = %p\n", (void*)sp);
    printf("  dl = %d\n", 0);
    
    // 调用ApeLoader
    ApeLoader(new_argc, sp, 0);
    
    // 不会执行到这里，因为ApeLoader是noreturn的
    munmap(base, st.st_size);
    free(stack_mem);
    return 0;
}
