// 大小端枚举
typedef enum {
    ENDIAN_LITTLE,
    ENDIAN_BIG
} endian_t;

static uint32_t READ32(const unsigned char* p, endian_t endian) {
    uint32_t val;
    if (endian == ENDIAN_LITTLE) {
        val = ((uint32_t)p[3] << 24) |
              ((uint32_t)p[2] << 16) |
              ((uint32_t)p[1] << 8) |
              ((uint32_t)p[0]);
    } else {
        val = ((uint32_t)p[0] << 24) |
              ((uint32_t)p[1] << 16) |
              ((uint32_t)p[2] << 8) |
              ((uint32_t)p[3]);
    }
    printf("Reading 32-bit value at offset %p (%s endian): %02x %02x %02x %02x = %u (0x%x)\n", 
           p, endian == ENDIAN_LITTLE ? "little" : "big",
           p[0], p[1], p[2], p[3], val, val);
    return val;
}

static int validate_ape_header(const unsigned char* raw, size_t file_size) {
    printf("APE header validation:\n");
    printf("  File size: 0x%x\n", (uint32_t)file_size);
    
    // 检查APE头的魔数
    if (memcmp(raw, "MZqFpD=", 7) != 0 &&
        memcmp(raw, "jartsr=", 7) != 0 &&
        memcmp(raw, "APEDBG=", 7) != 0) {
        printf("Invalid APE magic number\n");
        return 0;
    }
    
    // 检查APE头的结构
    if (raw[7] != '\'') {
        printf("Invalid APE header format (missing quote)\n");
        return 0;
    }
    
    // 检查Unix换行符(\n)
    if (raw[8] != '\n') {
        printf("Invalid APE header format (missing LF)\n");
        return 0;
    }
    
    // 读取size和elf_off (考虑\n后的偏移)
    uint32_t size = READ32(raw + 9, ENDIAN_LITTLE);
    uint32_t elf_off = READ32(raw + 13, ENDIAN_LITTLE);
    
    printf("  APE size: 0x%x\n", size);
    printf("  ELF offset: 0x%x\n", elf_off);
    
    printf("Raw bytes for size: %02x %02x %02x %02x\n", raw[9], raw[10], raw[11], raw[12]);
    printf("Raw bytes for elf_off: %02x %02x %02x %02x\n", raw[13], raw[14], raw[15], raw[16]);
    
    // 验证size和elf_off的合理性
    if (size > file_size || size < 0x1000) {
        printf("Invalid APE size: %u\n", size);
        return 0;
    }
    
    if (elf_off >= size || elf_off < 0x1000) {
        printf("Invalid ELF offset: %u\n", elf_off);
        return 0;
    }
    
    return 1;
}

static int parse_octal(const unsigned char* p, size_t size, size_t* pos, unsigned char* out) {
    if (*pos >= size) return 0;
    
    // 必须以\开头
    if (p[*pos] != '\\') return 0;
    (*pos)++;
    
    if (*pos >= size) return 0;
    
    // 解析八进制数
    int val = 0;
    int digits = 0;
    while (*pos < size && digits < 3 && 
           p[*pos] >= '0' && p[*pos] <= '7') {
        val = (val << 3) | (p[*pos] - '0');
        (*pos)++;
        digits++;
    }
    
    if (digits > 0) {
        *out = (unsigned char)val;
        printf("Decoded octal \\%03o to byte 0x%02x\n", val, val);
        return 1;
    }
    
    return 0;
}

static const Elf64_Ehdr* search_elf_header(const unsigned char* raw, 
                                                size_t file_size,
                                                uint32_t hint_offset) {
    printf("\nSearching for ELF header...\n");
    
    // 首先验证APE头
    if (!validate_ape_header(raw, file_size)) {
        printf("APE header validation failed\n");
        return NULL;
    }
    
    // 先尝试搜索printf语句
    const size_t search_size = file_size < 8192 ? file_size : 8192;
    printf("Searching first %zu bytes for printf statement...\n", search_size);
    
    static unsigned char elf_buffer[4096];
    const unsigned char* p = raw;
    const unsigned char* pe = raw + search_size;
    
    while (p + 8 <= pe) {
        // 搜索 printf '
        if (memcmp(p, "printf '", 8) == 0) {
            printf("Found printf statement at offset 0x%tx\n", p - raw);
            
            size_t i = 0;
            p += 8;
            
            // 解析printf内容直到结束引号
            while (p + 3 < pe && *p != '\'') {
                unsigned char c = *p++;
                if (c == '\\') {
                    if ('0' <= *p && *p <= '7') {
                        c = *p++ - '0';
                        if ('0' <= *p && *p <= '7') {
                            c = (c << 3) + (*p++ - '0');
                            if ('0' <= *p && *p <= '7') {
                                c = (c << 3) + (*p++ - '0');
                            }
                        }
                    }
                }
                if (i < sizeof(elf_buffer)) {
                    elf_buffer[i++] = c;
                }
            }
            
            // 检查是否有足够的数据构成ELF头
            if (i >= sizeof(Elf64_Ehdr)) {
                printf("Found potential ELF header in printf statement, size: %zu bytes\n", i);
                printf("First 16 bytes: ");
                for (size_t j = 0; j < 16; j++) {
                    printf("%02x ", elf_buffer[j]);
                }
                printf("\n");
                
                const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf_buffer;
                
                // 验证ELF头
                if (memcmp(ehdr->e_ident, "\x7f""ELF", 4) == 0 &&
                    ehdr->e_ident[EI_CLASS] == ELFCLASS64 &&
                    ehdr->e_ident[EI_DATA] == ELFDATA2LSB &&
                    (ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN) &&
                    ehdr->e_machine == EM_X86_64) {
                    printf("Found valid ELF header in printf statement\n");
                    return ehdr;
                }
                printf("Invalid ELF header in printf statement (failed validation)\n");
            }
        }
        p++;
    }
    
    // 如果在printf语句中没找到，尝试在指定偏移处查找
    uint32_t elf_off = READ32(raw + 13, ENDIAN_LITTLE);
    printf("Using ELF offset from APE header: 0x%x\n", elf_off);
    
    // 检查ELF头
    if (elf_off + sizeof(Elf64_Ehdr) <= file_size) {
        const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)(raw + elf_off);
        if (memcmp(ehdr->e_ident, "\x7f""ELF", 4) == 0) {
            printf("Found ELF header at offset 0x%x\n", elf_off);
            return ehdr;
        }
        printf("No valid ELF header at offset 0x%x\n", elf_off);
        
        // 打印ELF头的内容以便调试
        printf("ELF header bytes at 0x%x:\n", elf_off);
        for (size_t i = 0; i < sizeof(Elf64_Ehdr); i++) {
            printf("%02x ", raw[elf_off + i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        printf("\n");
    }
    
    printf("No valid ELF header found\n");
    return NULL;
}

static void dump_strings(const unsigned char* data, size_t size) {
    printf("\nSearching for strings...\n");
    size_t i = 0;
    while (i < size) {
        // Check if current byte is printable ASCII
        if (data[i] >= 32 && data[i] <= 126) {
            const unsigned char* start = &data[i];
            size_t len = 0;
            // Find string length
            while ((i + len) < size && data[i + len] >= 32 && data[i + len] <= 126) {
                len++;
            }
            // Only print strings of 4 or more chars
            if (len >= 4) {
                printf("String at 0x%zx: ", i);
                for (size_t j = 0; j < len; j++) {
                    putchar(start[j]);
                }
                printf("\n");
            }
            i += len;
        } else {
            i++;
        }
    }
    printf("\n");
}

static void* find_elf_header(const unsigned char* raw, size_t size) {
    // 首先检查 APE header
    if (size < 17 || (memcmp(raw, "MZqFpD=", 7) != 0 && 
                      memcmp(raw, "jartsr=", 7) != 0 && 
                      memcmp(raw, "APEDBG=", 7) != 0)) {
        printf("Error: Invalid APE magic number\n");
        return NULL;
    }

    // 检查引号和换行符
    if (raw[7] != '\'' || raw[8] != '\n') {
        printf("Error: Invalid APE header format\n");
        return NULL;
    }

    uint32_t ape_size = READ32(raw + 9, ENDIAN_LITTLE);
    uint32_t elf_off = READ32(raw + 13, ENDIAN_LITTLE);
    printf("APE header:\n");
    printf("  Magic: %.*s\n", 7, raw);
    printf("  Size: %u (0x%x)\n", ape_size, ape_size);
    printf("  ELF offset: %u (0x%x)\n", elf_off, elf_off);

    // 在前 8192 字节中搜索 printf 语句
    static unsigned char elf_buffer[4096];
    const unsigned char* p = raw;
    const unsigned char* pe = raw + (size < 8192 ? size : 8192);

    while (p + 8 <= pe) {
        // 搜索 printf '
        if (memcmp(p, "printf '", 8) == 0) {
            printf("Found printf statement at offset 0x%tx\n", p - raw);
            
            size_t i = 0;
            p += 8;
            
            // 解析 printf 内容直到结束引号
            while (p + 3 < pe && *p != '\'') {
                unsigned char c = *p++;
                if (c == '\\') {
                    if ('0' <= *p && *p <= '7') {
                        c = *p++ - '0';
                        if ('0' <= *p && *p <= '7') {
                            c = (c << 3) + (*p++ - '0');
                            if ('0' <= *p && *p <= '7') {
                                c = (c << 3) + (*p++ - '0');
                            }
                        }
                    }
                }
                if (i < sizeof(elf_buffer)) {
                    elf_buffer[i++] = c;
                }
            }
            
            // 检查是否有足够的数据构成 ELF 头
            if (i >= sizeof(Elf64_Ehdr)) {
                printf("Found %zu bytes in printf statement\n", i);
                printf("First 16 bytes: ");
                for (size_t j = 0; j < 16; j++) {
                    printf("%02x ", elf_buffer[j]);
                }
                printf("\n");
                
                const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf_buffer;
                
                // 验证 ELF 头
                if (memcmp(ehdr->e_ident, "\x7f""ELF", 4) == 0 &&
                    ehdr->e_ident[EI_CLASS] == ELFCLASS64 &&
                    ehdr->e_ident[EI_DATA] == ELFDATA2LSB &&
                    (ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN) &&
                    ehdr->e_machine == EM_X86_64) {
                    printf("Found valid ELF header in printf statement\n");
                    return (void*)ehdr;
                }
                printf("Invalid ELF header in printf statement (failed validation)\n");
                printf("  Magic: %02x %02x %02x %02x\n", 
                       ehdr->e_ident[0], ehdr->e_ident[1], 
                       ehdr->e_ident[2], ehdr->e_ident[3]);
                printf("  Class: %02x\n", ehdr->e_ident[EI_CLASS]);
                printf("  Data: %02x\n", ehdr->e_ident[EI_DATA]);
                printf("  Type: %04x\n", ehdr->e_type);
                printf("  Machine: %04x\n", ehdr->e_machine);
            }
        }
        p++;
    }
    
    printf("Error: No valid ELF header found\n");
    return NULL;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <target_file>\n", argv[0]);
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
    void* mapped_data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped_data == MAP_FAILED) {
        printf("Failed to map file\n");
        close(fd);
        return 1;
    }
    printf("Mapped at address: %p\n\n", mapped_data);

    // Dump strings from the file
    dump_strings((const unsigned char*)mapped_data, st.st_size);

    printf("\nAnalyzing APE header...\n");
    const unsigned char* raw = mapped_data;
    hexdump(raw, 0x40);

    // Read and validate APE header
    uint32_t size = READ32(raw + 9, ENDIAN_LITTLE);
    uint32_t elf_off = READ32(raw + 13, ENDIAN_LITTLE);
    printf("APE header validation:\n");
    printf("  File size: 0x%zx\n", st.st_size);
    printf("  APE size: 0x%x\n", size);
    printf("  ELF offset: 0x%x\n", elf_off);
    printf("Raw bytes for size: %02x %02x %02x %02x\n", raw[9], raw[10], raw[11], raw[12]);
    printf("Raw bytes for elf_off: %02x %02x %02x %02x\n", raw[13], raw[14], raw[15], raw[16]);

    // Validate APE header
    if (!validate_ape_header(raw, st.st_size)) {
        printf("Invalid APE header\n");
        munmap(mapped_data, st.st_size);
        close(fd);
        return 1;
    }

    // Search for ELF header
    printf("\nSearching for ELF header...\n");
    printf("Hint offset: 0x%x\n\n", elf_off);

    // Analyze file regions
    printf("Analyzing file regions...\n");
    printf("File start (first 128 bytes):\n");
    hexdump(raw, 128);

    printf("\nFile middle (around 0x8000):\n");
    if (st.st_size > 0x8000 + 128) {
        hexdump(raw + 0x8000, 128);
    }

    printf("\nFile end (last 128 bytes):\n");
    if (st.st_size >= 128) {
        hexdump(raw + st.st_size - 128, 128);
    }

    // Search for ELF header
    const Elf64_Ehdr* ehdr = search_elf_header(raw, st.st_size, elf_off);
    if (!ehdr) {
        printf("Error: Failed to find ELF header\n");
        munmap(mapped_data, st.st_size);
        close(fd);
        return 1;
    }

    // Print ELF header info
    printf("\nELF header found:\n");
    printf("  Magic: %02x %02x %02x %02x\n", 
           ehdr->e_ident[0], ehdr->e_ident[1], 
           ehdr->e_ident[2], ehdr->e_ident[3]);
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

    // Load segments
    if (!load_segments(raw, st.st_size, ehdr)) {
        printf("Error: Failed to load segments\n");
        munmap(mapped_data, st.st_size);
        close(fd);
        return 1;
    }

    munmap(mapped_data, st.st_size);
    close(fd);
    return 0;
} 