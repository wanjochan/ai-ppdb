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

// Parse octal escape sequence according to APE specification
static int parse_octal(const unsigned char* page, int i, int* pc) {
    int orig_i = i;
    int c = 0;
    
    // Parse up to three octal digits
    int digits = 0;
    while (digits < 3 && page[i] >= '0' && page[i] <= '7') {
        c = (c << 3) + (page[i] - '0');
        i++;
        digits++;
    }
    
    if (digits > 0) {
        *pc = c;
        return i - orig_i;  // 返回实际处理的字符数
    }
    return 0;  // 不是八进制序列
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
static const unsigned char* find_elf_header(const void* data, size_t size, size_t* out_size) {
    const unsigned char* p = data;
    const unsigned char* end = p + MIN(size, 8192);  // Only search first 8192 bytes
    static unsigned char elf_buffer[8192];  // Buffer for decoded ELF header and program headers
    int elf_pos = 0;
    const Elf64_Ehdr* ehdr = NULL;
    
    // First verify APE header
    const struct ApeHeader* ape = data;
    if (memcmp(ape->magic, APE_MAGIC_MZ, 7) != 0 &&
        memcmp(ape->magic, APE_MAGIC_UNIX, 7) != 0 &&
        memcmp(ape->magic, APE_MAGIC_DBG, 7) != 0) {
        printf("Invalid APE magic: %.7s\n", ape->magic);
        return NULL;
    }
    
    printf("APE header:\n");
    printf("  Magic: %.7s\n", ape->magic);
    printf("  Size: %u (0x%x)\n", ape->size, ape->size);
    printf("  ELF offset: %u (0x%x)\n", ape->elf_off, ape->elf_off);
    
    // Dump first 128 bytes for debugging
    printf("\nFirst 128 bytes of file:\n");
    for (int i = 0; i < 128; i += 16) {
        printf("  %04x:", i);
        for (int j = 0; j < 16 && i + j < 128; j++) {
            printf(" %02x", ((const unsigned char*)data)[i + j]);
        }
        printf("  ");
        for (int j = 0; j < 16 && i + j < 128; j++) {
            char c = ((const unsigned char*)data)[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        printf("\n");
    }
    
    // Skip APE header
    p += sizeof(struct ApeHeader);
    
    printf("\nSearching for printf statement...\n");
    
    // Search for printf statement with ELF header
    while (p < end - 4) {  // Need at least 4 bytes for minimal printf
        // Look for printf command
        if (memcmp(p, "printf", 6) == 0) {
            printf("\nFound printf at offset 0x%tx\n", p - (const unsigned char*)data);
            
            // Print context around printf
            printf("Context (32 bytes before):\n");
            for (int i = -32; i < 0; i++) {
                if (p + i >= (const unsigned char*)data) {
                    printf("%c", (p[i] >= 32 && p[i] <= 126) ? p[i] : '.');
                }
            }
            printf("\n");
            
            const unsigned char* printf_start = p;
            p += 6;
            
            // Skip whitespace
            while (p < end && (*p == ' ' || *p == '\t')) {
                printf("Skipping whitespace: 0x%02x\n", *p);
                p++;
            }
            
            // Check for single quote or double quote
            char quote = 0;
            if (*p == '\'' || *p == '"') {
                quote = *p;
                printf("Found quote: %c\n", quote);
                p++;
            }
            
            printf("Processing printf argument:\n");
            
            // Save start of argument for context printing
            const unsigned char* arg_start = p;
            
            // Process printf argument
            while (p < end && *p != quote && *p != '\n') {
                if (*p == '\\') {  // Found escape sequence
                    printf("Found escape at offset 0x%tx: ", p - (const unsigned char*)data);
                    p++;  // Skip backslash
                    if (p >= end) break;
                    
                    // Parse octal sequence
                    if ('0' <= *p && *p <= '7') {
                        printf("Found octal sequence at offset 0x%tx: ", p - (const unsigned char*)data);
                        int c;
                        const unsigned char* seq_start = p;
                        int chars_processed = parse_octal(p, 0, &c);
                        if (chars_processed > 0) {
                            printf("decoded 0x%02x ('%c') from sequence '", 
                                   c, (c >= 32 && c <= 126) ? c : '.');
                            // Print the actual sequence
                            for (int i = 0; i < chars_processed; i++) {
                                printf("%c", seq_start[i]);
                            }
                            printf("'\n");
                            
                            p += chars_processed;
                            if (elf_pos < sizeof(elf_buffer)) {
                                elf_buffer[elf_pos++] = (unsigned char)c;
                                
                                // Check if we have enough for ELF magic
                                if (elf_pos >= SELFMAG) {
                                    printf("Checking ELF magic at position %d:\n", elf_pos - SELFMAG);
                                    printf("Got:      ");
                                    for (int i = 0; i < SELFMAG; i++) {
                                        printf("%02x ", elf_buffer[elf_pos - SELFMAG + i]);
                                    }
                                    printf("\nExpected: ");
                                    for (int i = 0; i < SELFMAG; i++) {
                                        printf("%02x ", ELFMAG[i]);
                                    }
                                    printf("\n");
                                }
                                
                                // Check if we have enough for ELF header
                                if (elf_pos >= sizeof(Elf64_Ehdr)) {
                                    ehdr = (const Elf64_Ehdr*)elf_buffer;
                                    printf("\nGot complete ELF header (%zu bytes):\n", sizeof(Elf64_Ehdr));
                                    // Print raw bytes for debugging
                                    for (size_t i = 0; i < sizeof(Elf64_Ehdr); i++) {
                                        if (i % 16 == 0) printf("\n%04zx: ", i);
                                        printf("%02x ", elf_buffer[i]);
                                    }
                                    printf("\n");
                                    
                                    // Verify ELF header
                                    if (validate_elf_header(elf_buffer, elf_pos, &ehdr)) {
                                        printf("Found valid ELF header\n");
                                        if (out_size) *out_size = elf_pos;
                                        return elf_buffer;
                                    }
                                }
                            }
                        } else {
                            printf("failed to parse octal sequence starting with '%c'\n", *p);
                            p++;  // Skip invalid octal
                        }
                    } else {
                        printf("Skipping non-octal escape: %c\n", *p);
                        p++;  // Skip non-octal escape
                    }
                } else {
                    if (*p >= 32 && *p <= 126) {
                        printf("Skipping regular character: '%c'\n", *p);
                    } else {
                        printf("Skipping non-printable character: 0x%02x\n", *p);
                    }
                    p++;  // Skip regular character
                }
            }
            
            // Print the entire printf argument
            printf("\nComplete printf argument:\n");
            for (const unsigned char* s = arg_start; s < p; s++) {
                printf("%c", (*s >= 32 && *s <= 126) ? *s : '.');
            }
            printf("\n");
            
            printf("End of printf argument\n");
            
            // Reset buffer if this printf didn't contain valid ELF
            elf_pos = 0;
        }
        p++;
    }
    
    // If we didn't find it in printf statements, try the APE offset
    if (ape->elf_off > 0 && ape->elf_off + sizeof(Elf64_Ehdr) <= size) {
        const unsigned char* elf_data = (const unsigned char*)data + ape->elf_off;
        ehdr = (const Elf64_Ehdr*)elf_data;
        
        printf("\nTrying ELF header at APE offset 0x%x:\n", ape->elf_off);
        printf("  Magic: %02x %02x %02x %02x\n",
               ehdr->e_ident[0], ehdr->e_ident[1],
               ehdr->e_ident[2], ehdr->e_ident[3]);
        
        // Check ELF magic number
        if (memcmp(elf_data, ELFMAG, SELFMAG) != 0) {
            printf("Invalid ELF magic number:\n");
            printf("Expected: %02x %02x %02x %02x\n", 
                   ELFMAG[0], ELFMAG[1], ELFMAG[2], ELFMAG[3]);
            printf("Got:      %02x %02x %02x %02x\n", 
                   elf_data[0], elf_data[1], 
                   elf_data[2], elf_data[3]);
            return NULL;
        }
        
        if (ehdr->e_ident[EI_CLASS] == ELFCLASS64 &&
            ehdr->e_ident[EI_DATA] == ELFDATA2LSB &&
            ehdr->e_machine == EM_X86_64 &&
            (ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN) &&
            ehdr->e_phentsize == sizeof(Elf64_Phdr)) {
            printf("Found valid x86_64 ELF header at APE offset\n");
            if (out_size) *out_size = sizeof(Elf64_Ehdr);
            return elf_data;
        } else {
            printf("Invalid ELF header at APE offset:\n");
            printf("  Class: %d (expected %d)\n", ehdr->e_ident[EI_CLASS], ELFCLASS64);
            printf("  Data: %d (expected %d)\n", ehdr->e_ident[EI_DATA], ELFDATA2LSB);
            printf("  Machine: %d (expected %d)\n", ehdr->e_machine, EM_X86_64);
            printf("  Type: %d (expected %d or %d)\n", ehdr->e_type, ET_EXEC, ET_DYN);
            printf("  PHEntSize: %d (expected %d)\n", ehdr->e_phentsize, (int)sizeof(Elf64_Phdr));
        }
    }
    
    printf("\nNo valid ELF header found\n");
    return NULL;
}

// Load ELF segments into memory
static void* load_elf_segments(const void* data, size_t size) {
    // First check APE header
    const struct ApeHeader* ape = data;
    if (ape->elf_off == 0 || ape->elf_off >= size) {
        SET_ERROR("Invalid ELF offset in APE header: %u", ape->elf_off);
        return NULL;
    }

    // Get ELF header from APE offset
    const unsigned char* elf_header = (const unsigned char*)data + ape->elf_off;
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf_header;
    
    // Validate ELF header
    if (!validate_elf_header(elf_header, size - ape->elf_off, &ehdr)) {
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
    
    // Program headers are relative to the ELF header
    const Elf64_Phdr* phdr = (const Elf64_Phdr*)(elf_header + ehdr->e_phoff);
    
    printf("Program headers at offset: %lx\n", ehdr->e_phoff);
    printf("Number of program headers: %d\n", ehdr->e_phnum);
    
    // First pass: calculate memory requirements
    for (int i = 0; i < ehdr->e_phnum; i++) {
        printf("Program header %d:\n", i);
        printf("  Type: %x\n", phdr[i].p_type);
        printf("  Flags: %x\n", phdr[i].p_flags);
        printf("  Offset: %lx\n", phdr[i].p_offset);
        printf("  VAddr: %lx\n", phdr[i].p_vaddr);
        printf("  PAddr: %lx\n", phdr[i].p_paddr);
        printf("  FileSize: %lx\n", phdr[i].p_filesz);
        printf("  MemSize: %lx\n", phdr[i].p_memsz);
        printf("  Align: %lx\n", phdr[i].p_align);
        
        if (phdr[i].p_type == PT_LOAD) {
            size_t seg_start = ROUND_DOWN(phdr[i].p_vaddr, PAGE_SIZE);
            size_t seg_end = ROUND_UP(phdr[i].p_vaddr + phdr[i].p_memsz, PAGE_SIZE);
            
            printf("  Loadable segment: start=%lx, end=%lx\n", seg_start, seg_end);
            
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
    void* base = allocate_memory(total_size, PROT_READ | PROT_WRITE);
    if (!base) {
        return NULL;
    }

    printf("Allocated base memory at %p, size: %zu\n", base, total_size);

    // Second pass: load segments
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) {
            continue;
        }

        // Calculate segment addresses
        void* seg_addr = (char*)base + (phdr[i].p_vaddr - min_addr);
        size_t file_size = phdr[i].p_filesz;
        size_t mem_size = phdr[i].p_memsz;
        
        // File data is relative to the ELF header
        const void* file_data = elf_header + phdr[i].p_offset;

        printf("Loading segment %d:\n", i);
        printf("  vaddr=%lx\n", phdr[i].p_vaddr);
        printf("  file_size=%zu\n", file_size);
        printf("  mem_size=%zu\n", mem_size);
        printf("  file_offset=%lx\n", phdr[i].p_offset);
        printf("  seg_addr=%p\n", seg_addr);

        // Copy segment data
        if (file_size > 0) {
            if (phdr[i].p_offset + file_size > size - ape->elf_off) {
                SET_ERROR("Segment %d extends beyond file size", i);
                free_memory(base, total_size);
                return NULL;
            }
            memcpy(seg_addr, file_data, file_size);
            
            // Verify the copy
            printf("Verifying segment %d data:\n", i);
            hex_dump("  ", seg_addr, MIN(file_size, 64));
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


