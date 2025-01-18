static uint32_t READ32(const unsigned char* p) {
    uint32_t val = ((uint32_t)p[0]) |
                   ((uint32_t)p[1] << 8) |
                   ((uint32_t)p[2] << 16) |
                   ((uint32_t)p[3] << 24);
    printf("Reading 32-bit value at offset %p: %02x %02x %02x %02x = %u (0x%x)\n", 
           p, p[0], p[1], p[2], p[3], val, val);
    return val;
}

static int validate_ape_header(const unsigned char* raw, size_t file_size) {
    printf("APE header validation:\n");
    printf("  File size: 0x%x\n", (uint32_t)file_size);
    
    uint32_t size = READ32(raw + 8);
    uint32_t elf_off = READ32(raw + 12);
    
    printf("  APE size: 0x%x\n", size);
    printf("  ELF offset: 0x%x\n", elf_off);
    
    printf("Raw bytes for size: %02x %02x %02x %02x\n", raw[8], raw[9], raw[10], raw[11]);
    printf("Raw bytes for elf_off: %02x %02x %02x %02x\n", raw[12], raw[13], raw[14], raw[15]);
    
    if (size > file_size) {
        printf("Invalid APE size: %u > %u\n", size, (uint32_t)file_size);
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
    printf("\nSearching for ELF header in printf statements...\n");
    
    // 在前8192字节中搜索printf语句
    const size_t search_size = file_size < 8192 ? file_size : 8192;
    printf("Searching first %zu bytes for printf statement...\n", search_size);
    
    // 搜索printf语句的特征
    const char* printf_patterns[] = {
        "printf '\\177ELF",  // 标准格式
        "printf \"\\177ELF", // 双引号格式
        NULL
    };
    
    for (const char** pattern = printf_patterns; *pattern; pattern++) {
        size_t pattern_len = strlen(*pattern);
        
        for (size_t i = 0; i < search_size - pattern_len; i++) {
            if (memcmp(raw + i, *pattern, pattern_len) == 0) {
                printf("Found printf statement at offset 0x%zx\n", i);
                
                // 解析八进制转义序列
                unsigned char elf_header[sizeof(Elf64_Ehdr)];
                size_t elf_pos = 0;
                size_t pos = i + pattern_len;
                
                // 跳过printf语句开头的引号
                while (pos < search_size && raw[pos] != '\'' && raw[pos] != '"') pos++;
                if (pos >= search_size) continue;
                pos++; // 跳过引号
                
                while (pos < search_size && elf_pos < sizeof(Elf64_Ehdr)) {
                    if (raw[pos] == '\\') {
                        unsigned char val;
                        if (parse_octal(raw, search_size, &pos, &val)) {
                            elf_header[elf_pos++] = val;
                        }
                    } else if (raw[pos] == '\'' || raw[pos] == '"') {
                        break;
                    } else if (raw[pos] >= 32 && raw[pos] <= 126) {
                        // 直接复制可打印ASCII字符
                        elf_header[elf_pos++] = raw[pos++];
                    } else {
                        pos++;
                    }
                }
                
                if (elf_pos >= sizeof(Elf64_Ehdr)) {
                    printf("Successfully parsed ELF header from printf statement\n");
                    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf_header;
                    
                    // 打印ELF头的详细信息
                    printf("ELF header details:\n");
                    printf("  Magic: %02x %02x %02x %02x\n", 
                           ehdr->e_ident[0], ehdr->e_ident[1],
                           ehdr->e_ident[2], ehdr->e_ident[3]);
                    printf("  Class: %d\n", ehdr->e_ident[EI_CLASS]);
                    printf("  Data: %d\n", ehdr->e_ident[EI_DATA]);
                    printf("  Type: 0x%x\n", ehdr->e_type);
                    printf("  Machine: 0x%x\n", ehdr->e_machine);
                    
                    // 验证ELF头
                    if (memcmp(ehdr->e_ident, "\x7f""ELF", 4) == 0 &&
                        ehdr->e_ident[EI_CLASS] == ELFCLASS64 &&
                        ehdr->e_ident[EI_DATA] == ELFDATA2LSB &&
                        (ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN) &&
                        ehdr->e_machine == EM_X86_64) {
                        printf("Found valid ELF header\n");
                        return ehdr;
                    }
                    printf("Invalid ELF header (failed validation)\n");
                }
            }
        }
    }
    
    printf("No valid ELF header found in printf statements\n");
    
    // 如果在printf语句中找不到,尝试在hint_offset处查找
    if (hint_offset + sizeof(Elf64_Ehdr) <= file_size) {
        printf("Trying hint offset 0x%x...\n", hint_offset);
        const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)(raw + hint_offset);
        if (memcmp(ehdr->e_ident, "\x7f""ELF", 4) == 0) {
            printf("Found ELF header at hint offset\n");
            return ehdr;
        }
    }
    
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
    uint32_t size = READ32(raw + 8);
    uint32_t elf_off = READ32(raw + 12);
    printf("APE header validation:\n");
    printf("  File size: 0x%zx\n", st.st_size);
    printf("  APE size: 0x%x\n", size);
    printf("  ELF offset: 0x%x\n", elf_off);
    printf("Raw bytes for size: %02x %02x %02x %02x\n", raw[8], raw[9], raw[10], raw[11]);
    printf("Raw bytes for elf_off: %02x %02x %02x %02x\n", raw[12], raw[13], raw[14], raw[15]);

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

    // Search for ELF header in the file
    printf("\nSearching from APE header end (0x1000) for %zu bytes...\n\n", st.st_size - 0x1000);
    for (size_t offset = 0x1000; offset < st.st_size - 0x1000; offset += 0x1000) {
        printf("\nChecking at offset 0x%zx:\n", offset);
        hexdump(raw + offset, 64);
    }

    // Check for ELF header
    const unsigned char* elf_header = find_elf_header(raw, st.st_size, elf_off);
    if (!elf_header) {
        printf("No valid ELF header found\n");
        printf("Failed to locate valid ELF header\n");
        munmap(mapped_data, st.st_size);
        close(fd);
        return 1;
    }

    printf("Found valid ELF header at offset 0x%zx\n", elf_header - raw);
    printf("ELF header contents:\n");
    hexdump(elf_header, 64);

    // Clean up
    munmap(mapped_data, st.st_size);
    close(fd);
    return 0;
} 