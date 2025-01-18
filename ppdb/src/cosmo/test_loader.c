#include "cosmopolitan.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/auxv.h"

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

// ApeLoader的声明
extern void* ApeLoader(const char* path, long* sp, long* auxv, long pagesz, int os);

// 将八进制字符串转换为字节
static unsigned char oct_to_byte(const char* str) {
    unsigned char value = 0;
    for (int i = 0; i < 3 && str[i] >= '0' && str[i] <= '7'; i++) {
        value = value * 8 + (str[i] - '0');
    }
    return value;
}

// 将ELF头字符串转换为二进制
static void* convert_elf_str_to_bin(const char* str, size_t len) {
    unsigned char* bin = malloc(64);  // ELF头大小
    if (!bin) return NULL;
    
    memset(bin, 0, 64);
    int bin_pos = 0;
    
    for (int i = 0; i < len; i++) {
        if (str[i] == '\\') {
            i++;  // 跳过反斜杠
            if (str[i] >= '0' && str[i] <= '7') {
                // 八进制数字
                bin[bin_pos++] = oct_to_byte(str + i);
                while (i + 1 < len && str[i + 1] >= '0' && str[i + 1] <= '7') i++;
            }
        } else {
            bin[bin_pos++] = str[i];
        }
    }
    
    return bin;
}

// 查找ELF头部
static void* find_elf_header(void* base, size_t size) {
    // 打印前64字节的内容
    printf("First 64 bytes:\n");
    unsigned char* p = (unsigned char*)base;
    for (int i = 0; i < 64; i++) {
        printf("%02x ", p[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }

    // 搜索ELF头字符串
    const char* elf_str = "\\177ELF\\2\\1\\1\\011\\0\\0\\0\\0\\0\\0\\0\\0\\2\\0\\076\\0\\1\\0\\0\\0";
    size_t elf_str_len = strlen(elf_str);
    char* str_pos = NULL;
    
    for (int i = 0; i < size - elf_str_len; i++) {
        if (memcmp((char*)base + i, elf_str, elf_str_len) == 0) {
            str_pos = (char*)base + i;
            printf("Found ELF header string at offset 0x%x\n", i);
            break;
        }
    }

    if (!str_pos) {
        printf("No ELF header found\n");
        return NULL;
    }

    // 转换ELF头字符串为二进制
    void* elf_header = convert_elf_str_to_bin(str_pos, elf_str_len);
    if (!elf_header) {
        printf("Failed to convert ELF header string\n");
        return NULL;
    }

    // 设置内存保护
    size_t page_size = 65536;
    void* aligned_addr = (void*)((uintptr_t)elf_header & ~(page_size - 1));
    size_t aligned_size = ((size + page_size - 1) & ~(page_size - 1));
    
    printf("Setting protection for ELF header region:\n");
    printf("  Base address: %p\n", elf_header);
    printf("  Aligned address: %p\n", aligned_addr);
    printf("  Aligned size: %zu\n", aligned_size);
    
    if (mprotect(aligned_addr, aligned_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        printf("Failed to set protection: %s\n", strerror(errno));
        free(elf_header);
        return NULL;
    }

    // 验证ELF头
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)elf_header;
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

    // 设置程序头表的保护
    if (ehdr->e_phoff > 0 && ehdr->e_phnum > 0) {
        void* phdr = (void*)((char*)elf_header + ehdr->e_phoff);
        size_t phdr_size = ehdr->e_phnum * ehdr->e_phentsize;
        void* phdr_aligned = (void*)((uintptr_t)phdr & ~(page_size - 1));
        size_t phdr_aligned_size = ((phdr_size + page_size - 1) & ~(page_size - 1));
        
        printf("Setting protection for program header table:\n");
        printf("  Base address: %p\n", phdr);
        printf("  Aligned address: %p\n", phdr_aligned);
        printf("  Aligned size: %zu\n", phdr_aligned_size);
        
        if (mprotect(phdr_aligned, phdr_aligned_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
            printf("Failed to set protection for program header table: %s\n", strerror(errno));
            free(elf_header);
            return NULL;
        }
    }

    return elf_header;
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
    
    // 打印节头数量和字符串表索引
    printf("  SHnum: %d\n", ehdr->e_shnum);
    printf("  SHstrndx: %d\n", ehdr->e_shstrndx);
    
    // 获取页大小
    long page_size = sysconf(_SC_PAGESIZE);
    printf("Page size: %d\n", page_size);
    
    // 设置ELF头部区域的保护
    printf("Setting protection for ELF header region:\n");
    printf("  Base address: %p\n", elf_base);
    void* aligned_addr = (void*)((uintptr_t)elf_base & ~(page_size - 1));
    size_t aligned_size = ((sizeof(Elf64_Ehdr) + page_size - 1) & ~(page_size - 1));
    printf("  Aligned address: %p\n", aligned_addr);
    printf("  Aligned size: %zu\n", aligned_size);
    
    if (mprotect(aligned_addr, aligned_size, PROT_READ | PROT_WRITE) != 0) {
        printf("Failed to set protection for ELF header: %s\n", strerror(errno));
        return 1;
    }
    
    // 设置程序头表的保护
    Elf64_Phdr* phdr = (Elf64_Phdr*)((char*)elf_base + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            void* seg_addr = (void*)((uintptr_t)elf_base + phdr[i].p_offset);
            void* aligned_seg_addr = (void*)((uintptr_t)seg_addr & ~(page_size - 1));
            size_t aligned_seg_size = ((phdr[i].p_memsz + page_size - 1) & ~(page_size - 1));
            
            int prot = 0;
            if (phdr[i].p_flags & PF_R) prot |= PROT_READ;
            if (phdr[i].p_flags & PF_W) prot |= PROT_WRITE;
            if (phdr[i].p_flags & PF_X) prot |= PROT_EXEC;
            
            printf("Setting protection for segment %d:\n", i);
            printf("  Address: %p\n", seg_addr);
            printf("  Aligned address: %p\n", aligned_seg_addr);
            printf("  Size: %zu\n", aligned_seg_size);
            printf("  Flags: %x\n", prot);
            
            if (mprotect(aligned_seg_addr, aligned_seg_size, prot) != 0) {
                printf("Failed to set protection for segment %d: %s\n", i, strerror(errno));
                return 1;
            }
        }
    }
    
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
    sp[11] = 65536;  // auxv[3].a_val
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
    printf("  path = %s\n", argv[1]);
    printf("  sp = %p\n", sp);
    printf("  auxv = %p\n", sp + 4);
    printf("  pagesz = %ld\n", 65536L);
    printf("  os = %d\n", 0);
    printf("\n");
    
    // 调用ApeLoader
    void* result = ApeLoader(argv[1], sp, sp + 4, 65536, 0);
    
    // 这里不会执行,因为ApeLoader是noreturn函数
    printf("ApeLoader returned: %p\n", result);
    
    // 清理资源
    munmap(base, st.st_size);
    free(stack_mem);
    return 0;
}
