#include "cosmopolitan.h"
#include "libc/calls/struct/stat.h"  // 添加 stat 结构体定义

// Memory allocation and protection constants
#define PAGE_SIZE 4096
#define ROUND_UP(x, y) (((x) + (y) - 1) & -(y))
#define ROUND_DOWN(x, y) ((x) & -(y))

// APE magic definitions
#define APE_MAGIC_MZ "MZqFpD="
#define APE_MAGIC_UNIX "jartsr="
#define APE_MAGIC_DBG "APEDBG="

#pragma pack(push, 1)
struct ApeHeader {
    char magic[8];
    uint32_t size;
    uint32_t elf_off;
    uint8_t reserved[48];
};
#pragma pack(pop)

// Memory management functions
static void* allocate_memory(size_t size, int prot) {
    size_t aligned_size = ROUND_UP(size, PAGE_SIZE);
    void* ptr = mmap(NULL, aligned_size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        printf("Failed to allocate memory: size=%zu, prot=0x%x\n", size, prot);
        return NULL;
    }
    printf("Allocated memory: addr=%p, size=%zu, aligned_size=%zu\n", ptr, size, aligned_size);
    return ptr;
}

static int protect_memory(void* addr, size_t size, int prot) {
    size_t aligned_size = ROUND_UP(size, PAGE_SIZE);
    if (mprotect(addr, aligned_size, prot) != 0) {
        printf("Failed to protect memory: addr=%p, size=%zu, prot=0x%x\n", addr, size, prot);
        return -1;
    }
    printf("Protected memory: addr=%p, size=%zu, prot=0x%x\n", addr, size, prot);
    return 0;
}

static int free_memory(void* addr, size_t size) {
    size_t aligned_size = ROUND_UP(size, PAGE_SIZE);
    if (munmap(addr, aligned_size) != 0) {
        printf("Failed to free memory: addr=%p, size=%zu\n", addr, size);
        return -1;
    }
    printf("Freed memory: addr=%p, size=%zu\n", addr, size);
    return 0;
}

// Convert ELF protection flags to system protection flags
static int elf_to_sys_prot(int elf_flags) {
    int prot = PROT_NONE;
    if (elf_flags & PF_R) prot |= PROT_READ;
    if (elf_flags & PF_W) prot |= PROT_WRITE;
    if (elf_flags & PF_X) prot |= PROT_EXEC;
    return prot;
}

// ApeLoader的声明
extern void* ApeLoader(const char* path, long* sp, long* auxv, long pagesz, int os);

//-----------------------------------------------------------------------------
// 辅助函数
//-----------------------------------------------------------------------------

// 以十六进制格式打印内存区域的内容
static inline void hex_dump(const char* prefix, const void* data, size_t size) {
    const unsigned char* p = data;
    printf("%s Dumping %zu bytes from %p:\n", prefix, size, data);
    for (size_t i = 0; i < size; i += 16) {
        printf("%s%04zx:", prefix, i);
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                printf(" %02x", p[i + j]);
            } else {
                printf("   ");
            }
        }
        printf("  ");
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            char c = p[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
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

// Error handling macros and functions
#define ERROR_BUFFER_SIZE 256
static char error_buffer[ERROR_BUFFER_SIZE];

#define SET_ERROR(fmt, ...) do { \
    snprintf(error_buffer, ERROR_BUFFER_SIZE, fmt, ##__VA_ARGS__); \
    printf("Error: %s\n", error_buffer); \
} while(0)

#define CHECK_NULL(ptr, msg, ...) do { \
    if (!(ptr)) { \
        SET_ERROR(msg, ##__VA_ARGS__); \
        return 0; \
    } \
} while(0)

#define CHECK_COND(cond, msg, ...) do { \
    if (!(cond)) { \
        SET_ERROR(msg, ##__VA_ARGS__); \
        return 0; \
    } \
} while(0)

// Resource cleanup structure
struct LoaderContext {
    void* base_address;
    size_t total_size;
    const unsigned char* elf_data;
    size_t elf_size;
    void* entry_point;
};

static void cleanup_context(struct LoaderContext* ctx) {
    if (!ctx) return;
    if (ctx->base_address) {
        free_memory(ctx->base_address, ctx->total_size);
    }
    memset(ctx, 0, sizeof(*ctx));
}

// Validate ELF header with detailed checks
static int validate_elf_header(const unsigned char* elf_data, size_t elf_size, 
                             const Elf64_Ehdr** out_ehdr) {
    CHECK_COND(elf_size >= sizeof(Elf64_Ehdr), 
              "ELF data too small: %zu < %zu", elf_size, sizeof(Elf64_Ehdr));
    
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf_data;
    
    printf("ELF header at %p:\n", elf_data);
    printf("  Magic: %02x %02x %02x %02x\n",
           ehdr->e_ident[0], ehdr->e_ident[1], ehdr->e_ident[2], ehdr->e_ident[3]);
    printf("  Class: %02x\n", ehdr->e_ident[EI_CLASS]);
    printf("  Data: %02x\n", ehdr->e_ident[EI_DATA]);
    printf("  Version: %02x\n", ehdr->e_ident[EI_VERSION]);
    printf("  Type: %04x\n", ehdr->e_type);
    printf("  Machine: %04x\n", ehdr->e_machine);
    printf("  Entry: %016lx\n", ehdr->e_entry);
    printf("  PHoff: %016lx\n", ehdr->e_phoff);
    printf("  SHoff: %016lx\n", ehdr->e_shoff);
    printf("  Flags: %08x\n", ehdr->e_flags);
    printf("  EHSize: %04x\n", ehdr->e_ehsize);
    printf("  PHEntSize: %04x\n", ehdr->e_phentsize);
    printf("  PHNum: %04x\n", ehdr->e_phnum);
    printf("  SHEntSize: %04x\n", ehdr->e_shentsize);
    printf("  SHNum: %04x\n", ehdr->e_shnum);
    printf("  SHStrNdx: %04x\n", ehdr->e_shstrndx);
    
    // Check magic number
    CHECK_COND(memcmp(ehdr->e_ident, ELFMAG, SELFMAG) == 0,
              "Invalid ELF magic number");
    
    // Check class (64-bit)
    CHECK_COND(ehdr->e_ident[EI_CLASS] == ELFCLASS64,
              "Not a 64-bit ELF file");
    
    // Check data encoding
    CHECK_COND(ehdr->e_ident[EI_DATA] == ELFDATA2LSB,
              "Not little-endian");
    
    // Check version
    CHECK_COND(ehdr->e_ident[EI_VERSION] == EV_CURRENT,
              "Invalid ELF version");
    
    // Check type
    CHECK_COND(ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN,
              "Not an executable or shared object");
    
    // Check machine
    CHECK_COND(ehdr->e_machine == EM_X86_64,
              "Not x86_64 architecture");
    
    // Check program header
    CHECK_COND(ehdr->e_phoff > 0 && ehdr->e_phoff < elf_size,
              "Invalid program header offset");
    CHECK_COND(ehdr->e_phentsize == sizeof(Elf64_Phdr),
              "Invalid program header size");
    CHECK_COND(ehdr->e_phnum > 0,
              "No program headers");
    
    // Check entry point
    CHECK_COND(ehdr->e_entry > 0,
              "Invalid entry point");
    
    if (out_ehdr) *out_ehdr = ehdr;
    return 1;
}

//-----------------------------------------------------------------------------
// ELF头搜索
//-----------------------------------------------------------------------------

// Find embedded ELF header in printf statement
static const unsigned char* find_elf_header(const void* data, size_t size) {
    const unsigned char* p = data;
    const unsigned char* end = p + MIN(size, 8192);  // Only search first 8192 bytes
    
    printf("\nSearching for ELF header in first 8192 bytes:\n");
    
    // Look for printf statement with ELF magic
    while (p < end - 16) {
        // Skip to printf
        if (memcmp(p, "printf", 6) == 0) {
            printf("Found printf at offset: %td\n", p - (const unsigned char*)data);
            
            p += 6;
            // Skip whitespace and quote
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\'' || *p == '"')) p++;
            
            // Print next few bytes for debugging
            printf("Next bytes after printf: ");
            for (int i = 0; i < 16 && p + i < end; i++) {
                printf("%c", p[i] >= 32 && p[i] <= 126 ? p[i] : '.');
            }
            printf("\nHex: ");
            for (int i = 0; i < 16 && p + i < end; i++) {
                printf("%02x ", p[i]);
            }
            printf("\n");
            
            // Check for octal escape sequence for ELF magic
            if (p[0] == '\\' && p[1] == '1' && p[2] == '7' && p[3] == '7') {
                printf("Found potential ELF magic octal sequence\n");
                // Found potential ELF header
                unsigned char* elf_header = malloc(sizeof(Elf64_Ehdr));
                if (!elf_header) {
                    printf("Failed to allocate memory for ELF header\n");
                    return NULL;
                }
                
                size_t elf_pos = 0;
                const unsigned char* parse_start = p;
                
                // Parse octal escape sequences
                while (p < end && elf_pos < sizeof(Elf64_Ehdr)) {
                    if (*p == '\\') {
                        p++;
                        int c;
                        int new_pos = parse_octal((const unsigned char*)p, 0, &c);
                        if (new_pos > 0) {
                            p += new_pos;
                            elf_header[elf_pos++] = c;
                        } else {
                            p++;
                        }
                    } else if (*p == '\'' || *p == '"') {
                        break;
                    } else {
                        p++;
                    }
                }
                
                // Verify ELF magic
                if (elf_pos >= 4 && 
                    elf_header[0] == 0x7f && 
                    elf_header[1] == 'E' && 
                    elf_header[2] == 'L' && 
                    elf_header[3] == 'F') {
                    printf("Found ELF header in printf statement\n");
                    return elf_header;
                }
                
                free(elf_header);
            }
        }
        p++;
    }
    
    // If not found in printf, try direct ELF header at APE offset
    const struct ApeHeader* ape = data;
    printf("\nChecking APE header at offset 0:\n");
    printf("  Magic: %.8s\n", ape->magic);
    printf("  Size: %u (0x%x)\n", ape->size, ape->size);
    printf("  ELF offset: %u (0x%x)\n", ape->elf_off, ape->elf_off);
    
    if (ape->elf_off && ape->elf_off < size) {
        const unsigned char* elf = (const unsigned char*)data + ape->elf_off;
        printf("\nChecking for ELF header at offset %u (0x%x):\n", ape->elf_off, ape->elf_off);
        hex_dump("  ", elf, 64);
        
        if (elf[0] == 0x7f && 
            elf[1] == 'E' && 
            elf[2] == 'L' && 
            elf[3] == 'F') {
            printf("Found ELF header at APE offset: %u\n", ape->elf_off);
            return elf;
        }
        printf("Invalid ELF magic at APE offset %u: %02x %02x %02x %02x\n",
               ape->elf_off, elf[0], elf[1], elf[2], elf[3]);
    }
    
    printf("No valid ELF header found\n");
    return NULL;
}

// Load ELF segments into memory
static void* load_elf_segments(const unsigned char* elf_data, size_t elf_size) {
    // First try to find embedded ELF header
    const unsigned char* elf_header = find_elf_header(elf_data, elf_size);
    if (!elf_header) {
        SET_ERROR("Could not find valid ELF header");
        return NULL;
    }
    
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf_header;
    if (!validate_elf_header(elf_header, elf_size - (elf_header - elf_data), &ehdr)) {
        return NULL;
    }

    // Validate program headers
    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
        SET_ERROR("No program headers found");
        return NULL;
    }

    if (ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
        SET_ERROR("Invalid program header size: %u != %zu", 
                 ehdr->e_phentsize, sizeof(Elf64_Phdr));
        return NULL;
    }

    // Calculate total memory size needed
    size_t min_addr = (size_t)-1;
    size_t max_addr = 0;
    const Elf64_Phdr* phdr = (const Elf64_Phdr*)(elf_header + ehdr->e_phoff);
    
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            size_t seg_start = ROUND_DOWN(phdr[i].p_vaddr, PAGE_SIZE);
            size_t seg_end = ROUND_UP(phdr[i].p_vaddr + phdr[i].p_memsz, PAGE_SIZE);
            
            if (seg_start < min_addr) min_addr = seg_start;
            if (seg_end > max_addr) max_addr = seg_end;
        }
    }

    if (min_addr == (size_t)-1 || max_addr == 0) {
        SET_ERROR("No loadable segments found");
        return NULL;
    }

    // Allocate memory for all segments
    size_t total_size = max_addr - min_addr;
    void* base = allocate_memory(total_size, elf_to_sys_prot(ehdr->e_flags));
    if (!base) {
        return NULL;
    }

    printf("Allocated base memory at %p, size: %zu\n", base, total_size);

    // Load segments
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) {
            continue;
        }

        // Calculate segment addresses
        void* seg_addr = (char*)base + (phdr[i].p_vaddr - min_addr);
        size_t file_size = phdr[i].p_filesz;
        size_t mem_size = phdr[i].p_memsz;

        printf("Loading segment %d: vaddr=%lx, file_size=%zu, mem_size=%zu\n",
               i, phdr[i].p_vaddr, file_size, mem_size);

        // Copy segment data
        if (file_size > 0) {
            if (phdr[i].p_offset + file_size > elf_size) {
                SET_ERROR("Segment %d extends beyond file size", i);
                free_memory(base, total_size);
                return NULL;
            }
            memcpy(seg_addr, elf_header + phdr[i].p_offset, file_size);
        }

        // Zero out remaining memory (BSS)
        if (mem_size > file_size) {
            memset((char*)seg_addr + file_size, 0, mem_size - file_size);
        }

        // Set proper segment permissions
        uint32_t prot = elf_to_sys_prot(phdr[i].p_flags);
        if (protect_memory(seg_addr, mem_size, prot) != 0) {
            free_memory(base, total_size);
            return NULL;
        }
    }

    // Return entry point
    return (char*)base + (ehdr->e_entry - min_addr);
}

int main(int argc, char* argv[]) {
    struct LoaderContext ctx = {0};
    const char* target_path = (argc > 1) ? argv[1] : "test_target.exe";
    
    printf("Loading target: %s\n", target_path);
    
    // Open the file
    int fd = open(target_path, O_RDONLY);
    if (fd < 0) {
        SET_ERROR("Failed to open file: %s (error: %d)", target_path, errno);
        return 1;
    }
    
    // Get file size using fstat
    struct stat st;
    if (fstat(fd, &st) != 0) {
        SET_ERROR("Failed to get file size");
        close(fd);
        return 1;
    }
    int64_t file_size = st.st_size;
    
    printf("File size: %ld bytes\n", file_size);
    
    // Map the file
    void* file_data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_data == MAP_FAILED) {
        SET_ERROR("Failed to map file");
        close(fd);
        return 1;
    }
    
    printf("\nFile header dump:\n");
    hex_dump("  ", file_data, 128);
    
    // Validate APE header
    const struct ApeHeader* ape = file_data;
    if (memcmp(ape->magic, APE_MAGIC_MZ, 7) != 0 &&
        memcmp(ape->magic, APE_MAGIC_UNIX, 7) != 0 &&
        memcmp(ape->magic, APE_MAGIC_DBG, 7) != 0) {
        SET_ERROR("Invalid APE magic number");
        munmap(file_data, file_size);
        close(fd);
        return 1;
    }
    
    printf("APE header:\n");
    printf("  Magic: %.7s\n", ape->magic);
    printf("  Size: %u (0x%x)\n", ape->size, ape->size);
    printf("  ELF offset: %u (0x%x)\n", ape->elf_off, ape->elf_off);
    
    // Load segments
    ctx.elf_data = file_data;
    ctx.elf_size = file_size;
    ctx.entry_point = load_elf_segments(file_data, file_size);
    
    if (!ctx.entry_point) {
        SET_ERROR("Failed to load segments");
        cleanup_context(&ctx);
        munmap(file_data, file_size);
        close(fd);
        return 1;
    }
    
    printf("Successfully loaded segments\n");
    printf("Entry point: %p\n", ctx.entry_point);
    
    // Clean up file mapping
    munmap(file_data, file_size);
    close(fd);
    
    // Execute the loaded program
    typedef int (*entry_func_t)(int argc, char* argv[]);
    entry_func_t entry = (entry_func_t)ctx.entry_point;
    
    printf("Executing loaded program...\n\n");
    int ret = entry(argc, argv);
    printf("\nProgram returned: %d\n", ret);
    
    // Clean up
    cleanup_context(&ctx);
    return ret;
}

