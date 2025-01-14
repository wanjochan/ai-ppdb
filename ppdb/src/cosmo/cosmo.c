#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

/* PLT存根代码 */
static unsigned char plt_stub[] = {
    0x68, 0x00, 0x00, 0x00, 0x00,  /* push $index */
    0xff, 0x25, 0x00, 0x00, 0x00, 0x00  /* jmp *0(%rip) */
};

/* PLT解析器代码 */
static unsigned char plt_resolver[] = {
    0x53,                       /* push %rbx */
    0x48, 0x89, 0xe3,          /* mov %rsp, %rbx */
    0x48, 0x83, 0xec, 0x20,    /* sub $0x20, %rsp */
    0x48, 0x8b, 0x7b, 0x08,    /* mov 0x8(%rbx), %rdi */
    0x48, 0x8b, 0x73, 0x10,    /* mov 0x10(%rbx), %rsi */
    0xe8, 0x00, 0x00, 0x00, 0x00, /* call resolve_plt */
    0x48, 0x83, 0xc4, 0x20,    /* add $0x20, %rsp */
    0x5b,                       /* pop %rbx */
    0x48, 0x83, 0xc4, 0x08,    /* add $0x8, %rsp */
    0xff, 0xe0                  /* jmp *%rax */
};

/* GOT/PLT表项结构 */
struct got_entry {
    const char* name;    /* 符号名 */
    void* got_addr;     /* GOT表项地址 */
    void* plt_entry;    /* PLT入口点 */
    void* sym_addr;     /* 符号地址 */
    int is_external;    /* 是否是外部符号 */
    int index;          /* PLT表项索引 */
};

/* 加载信息 */
struct load_info {
    void* base;        /* 映射基址 */
    void* obj_base;    /* 目标文件基址 */
    size_t size;       /* 总大小 */
    size_t obj_size;   /* 目标文件大小 */
    int has_ape;
    
    /* GOT/PLT表 */
    struct got_entry* got_table;  /* GOT表 */
    size_t got_size;             /* GOT表大小 */
    size_t got_count;            /* GOT表项数量 */
    void* got_base;              /* GOT段基址 */
    void* plt_base;              /* PLT段基址 */
};

/* 全局加载信息 */
static struct load_info g_info = {0};

/* 函数声明 */
static void* find_symbol(const struct load_info* info, const char* name);
static void show_usage(const char* prog_name);

/* 初始化GOT/PLT表 */
static int init_got_table(struct load_info* info) {
    /* 分配初始GOT表空间 */
    info->got_size = 1024;  /* 初始大小 */
    info->got_count = 0;
    info->got_table = calloc(info->got_size, sizeof(struct got_entry));
    if (!info->got_table) {
        dprintf(2, "Failed to allocate GOT table\n");
        return -1;
    }
    
    /* 分配GOT段空间 */
    info->got_base = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (info->got_base == MAP_FAILED) {
        dprintf(2, "Failed to allocate GOT segment\n");
        free(info->got_table);
        return -1;
    }
    
    /* 分配PLT段空间 */
    info->plt_base = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (info->plt_base == MAP_FAILED) {
        dprintf(2, "Failed to allocate PLT segment\n");
        munmap(info->got_base, 4096);
        free(info->got_table);
        return -1;
    }
    
    /* 复制PLT解析器代码 */
    memcpy(info->plt_base, plt_resolver, sizeof(plt_resolver));
    
    return 0;
}

/* 创建PLT存根 */
static void* create_plt_stub(struct load_info* info, struct got_entry* entry) {
    static size_t plt_offset = sizeof(plt_resolver);  /* 跳过解析器代码 */
    
    /* 检查PLT段空间 */
    if (plt_offset + sizeof(plt_stub) > 4096) {
        dprintf(2, "PLT segment full\n");
        return NULL;
    }
    
    /* 复制PLT存根代码 */
    void* stub = (char*)info->plt_base + plt_offset;
    memcpy(stub, plt_stub, sizeof(plt_stub));
    
    /* 设置PLT索引和跳转目标 */
    *(uint32_t*)(stub + 1) = entry->index;
    *(uint32_t*)(stub + 7) = (uint32_t)((uint64_t)info->plt_base - ((uint64_t)stub + 11));
    
    plt_offset += sizeof(plt_stub);
    return stub;
}

/* 在GOT表中查找或添加符号 */
static struct got_entry* get_got_entry(struct load_info* info, const char* name, int is_external) {
    /* 查找已存在的表项 */
    for (size_t i = 0; i < info->got_count; i++) {
        if (strcmp(info->got_table[i].name, name) == 0) {
            return &info->got_table[i];
        }
    }
    
    /* 检查是否需要扩展表 */
    if (info->got_count >= info->got_size) {
        size_t new_size = info->got_size * 2;
        struct got_entry* new_table = realloc(info->got_table, 
                                            new_size * sizeof(struct got_entry));
        if (!new_table) {
            dprintf(2, "Failed to expand GOT table\n");
            return NULL;
        }
        info->got_table = new_table;
        info->got_size = new_size;
    }
    
    /* 添加新表项 */
    struct got_entry* entry = &info->got_table[info->got_count];
    entry->name = strdup(name);  /* 复制符号名 */
    entry->is_external = is_external;
    entry->sym_addr = NULL;      /* 初始化符号地址为NULL */
    entry->index = info->got_count++;
    
    /* 分配GOT表项 */
    size_t offset = entry->index * 8;
    if (offset < 4096) {  /* 确保不超过GOT段大小 */
        entry->got_addr = (char*)info->got_base + offset;
        
        /* 为外部符号创建PLT存根 */
        if (is_external) {
            entry->plt_entry = create_plt_stub(info, entry);
            if (!entry->plt_entry) {
                dprintf(2, "Failed to create PLT stub for %s\n", name);
                return NULL;
            }
            /* 设置GOT表项指向PLT存根 */
            *(void**)entry->got_addr = entry->plt_entry;
        } else {
            entry->plt_entry = NULL;
            /* 初始化GOT表项为NULL，等待重定位时设置 */
            *(void**)entry->got_addr = NULL;
        }
    } else {
        dprintf(2, "GOT segment full\n");
        return NULL;
    }
    
    return entry;
}

/* 加载静态库 */
static struct load_info* load_lib(const char* path) {
    /* 初始化全局信息 */
    memset(&g_info, 0, sizeof(g_info));
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        dprintf(2, "Failed to open %s: %s\n", path, strerror(errno));
        return NULL;
    }

    /* 获取文件大小 */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        dprintf(2, "Failed to stat %s: %s\n", path, strerror(errno));
        close(fd);
        return NULL;
    }

    /* 计算对齐后的大小 */
    size_t aligned_size = (st.st_size + 4095) & ~4095;
    dprintf(1, "File size: %ld, aligned size: %ld\n", st.st_size, aligned_size);

    /* 映射文件 */
    void* base = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        dprintf(2, "Failed to allocate memory: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    dprintf(1, "Memory mapped at %p\n", base);

    /* 读取文件内容 */
    ssize_t bytes_read = read(fd, base, st.st_size);
    close(fd);

    if (bytes_read != st.st_size) {
        dprintf(2, "Failed to read file: %s\n", strerror(errno));
        munmap(base, aligned_size);
        return NULL;
    }
    dprintf(1, "Read %ld bytes from file\n", bytes_read);

    /* 初始化GOT表 */
    g_info.base = base;
    g_info.size = aligned_size;
    if (init_got_table(&g_info) != 0) {
        munmap(base, aligned_size);
        return NULL;
    }

    /* 检查归档头 */
    const char* ar_magic = "!<arch>\n";
    if (memcmp(base, ar_magic, 8) != 0) {
        dprintf(2, "Invalid archive magic in %s\n", path);
        munmap(base, aligned_size);
        free(g_info.got_table);
        return NULL;
    }

    /* 处理归档成员 */
    char* p = (char*)base + 8;
    struct ar_hdr* hdr = (struct ar_hdr*)p;
    
    /* 检查是否是符号表 */
    if (strncmp(hdr->ar_name, "/               ", 16) == 0) {
        /* 跳过符号表 */
        char size_str[11] = {0};
        memcpy(size_str, hdr->ar_size, 10);
        size_t size = atoi(size_str);
        p += sizeof(struct ar_hdr) + ((size + 1) & ~1);
        hdr = (struct ar_hdr*)p;
    }
    
    /* 检查是否是长文件名表 */
    if (strncmp(hdr->ar_name, "//              ", 16) == 0) {
        /* 跳过长文件名表 */
        char size_str[11] = {0};
        memcpy(size_str, hdr->ar_size, 10);
        size_t size = atoi(size_str);
        p += sizeof(struct ar_hdr) + ((size + 1) & ~1);
        hdr = (struct ar_hdr*)p;
    }
    
    /* 检查ELF文件 */
    p += sizeof(struct ar_hdr);
    if (memcmp(p, ELFMAG, SELFMAG) != 0) {
        dprintf(2, "Member is not an ELF file\n");
        munmap(base, aligned_size);
        free(g_info.got_table);
        return NULL;
    }

    /* 设置加载信息 */
    g_info.obj_base = p;
    g_info.obj_size = st.st_size - (p - (char*)base);
    g_info.has_ape = 0;
    dprintf(1, "Base address: %p, object base: %p\n", g_info.base, g_info.obj_base);

    return &g_info;
}

/* 查找符号 */
static void* find_symbol(const struct load_info* info, const char* name) {
    if (!info || !info->obj_base) {
        return NULL;
    }

    /* 解析ELF头 */
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)info->obj_base;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        dprintf(2, "Invalid ELF magic\n");
        return NULL;
    }

    /* 查找节头表 */
    Elf64_Shdr* shdr = (Elf64_Shdr*)((char*)info->obj_base + ehdr->e_shoff);
    
    /* 查找符号表和字符串表 */
    Elf64_Sym* symtab = NULL;
    char* strtab = NULL;
    size_t symcount = 0;
    Elf64_Shdr* sym_shdr = NULL;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            symtab = (Elf64_Sym*)((char*)info->obj_base + shdr[i].sh_offset);
            symcount = shdr[i].sh_size / sizeof(Elf64_Sym);
            sym_shdr = &shdr[i];
            dprintf(1, "Found symbol table at section %d: %ld symbols\n", i, symcount);
            
            /* 字符串表在符号表之后 */
            if (i + 1 < ehdr->e_shnum) {
                strtab = (char*)info->obj_base + shdr[i + 1].sh_offset;
            }
            break;
        }
    }
    
    if (!symtab || !strtab) {
        dprintf(2, "Symbol table or string table not found\n");
        return NULL;
    }
    
    /* 在符号表中查找 */
    for (size_t i = 0; i < symcount; i++) {
        const char* sym_name = strtab + symtab[i].st_name;
        if (strcmp(sym_name, name) == 0) {
            /* 检查符号类型和绑定 */
            unsigned char type = ELF64_ST_TYPE(symtab[i].st_info);
            unsigned char bind = ELF64_ST_BIND(symtab[i].st_info);
            
            dprintf(1, "Found symbol %s: type=%d, bind=%d, shndx=%d\n", 
                   name, type, bind, symtab[i].st_shndx);
            
            /* 检查符号是否在有效的节中 */
            if (symtab[i].st_shndx == SHN_UNDEF) {
                dprintf(2, "Symbol %s is undefined\n", name);
                return NULL;
            }
            
            if (symtab[i].st_shndx >= ehdr->e_shnum) {
                dprintf(2, "Symbol %s has invalid section index %d\n", 
                       name, symtab[i].st_shndx);
                return NULL;
            }
            
            /* 获取符号所在的节 */
            Elf64_Shdr* sym_section = &shdr[symtab[i].st_shndx];
            dprintf(1, "Symbol section: type=%d, offset=%ld, size=%ld, flags=%ld\n",
                   sym_section->sh_type, sym_section->sh_offset,
                   sym_section->sh_size, sym_section->sh_flags);
            
            /* 计算符号的实际地址 */
            void* sym_addr = (char*)info->obj_base + symtab[i].st_value;
            dprintf(1, "Symbol address: %p\n", sym_addr);
            return sym_addr;
        }
    }
    
    dprintf(2, "Symbol %s not found\n", name);
    return NULL;
}

/* PLT解析器函数 */
static void* resolve_plt(int index, void* caller) {
    if (index < 0 || index >= g_info.got_count) {
        dprintf(2, "Invalid PLT index: %d\n", index);
        return NULL;
    }
    
    /* 获取GOT表项 */
    struct got_entry* entry = &g_info.got_table[index];
    if (!entry->name) {
        dprintf(2, "Invalid GOT entry at index %d\n", index);
        return NULL;
    }
    
    /* 查找符号 */
    if (!entry->sym_addr) {
        entry->sym_addr = find_symbol(&g_info, entry->name);
        if (!entry->sym_addr) {
            dprintf(2, "Failed to resolve symbol %s\n", entry->name);
            return NULL;
        }
        /* 更新GOT表项 */
        *(void**)entry->got_addr = entry->sym_addr;
    }
    
    return entry->sym_addr;
}

/* 处理重定位 */
static int process_relocs(const struct load_info* info) {
    if (!info || !info->obj_base) {
        return -1;
    }

    /* 解析ELF头 */
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)info->obj_base;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        dprintf(2, "Invalid ELF magic\n");
        return -1;
    }

    /* 查找节头表 */
    Elf64_Shdr* shdr = (Elf64_Shdr*)((char*)info->obj_base + ehdr->e_shoff);
    
    /* 遍历所有节,查找重定位节 */
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_RELA) {
            dprintf(1, "Found relocation section %d: %ld entries\n", 
                   i, shdr[i].sh_size / sizeof(Elf64_Rela));
            
            /* 获取重定位表 */
            Elf64_Rela* rela = (Elf64_Rela*)((char*)info->obj_base + shdr[i].sh_offset);
            size_t relcount = shdr[i].sh_size / sizeof(Elf64_Rela);
            
            /* 获取符号表 */
            Elf64_Shdr* symtab_shdr = &shdr[shdr[i].sh_link];
            Elf64_Sym* symtab = (Elf64_Sym*)((char*)info->obj_base + symtab_shdr->sh_offset);
            
            /* 获取字符串表 */
            char* strtab = (char*)info->obj_base + shdr[symtab_shdr->sh_link].sh_offset;
            
            /* 处理每个重定位项 */
            for (size_t j = 0; j < relcount; j++) {
                Elf64_Rela* rel = &rela[j];
                Elf64_Sym* sym = &symtab[ELF64_R_SYM(rel->r_info)];
                const char* sym_name = strtab + sym->st_name;
                unsigned long r_type = ELF64_R_TYPE(rel->r_info);
                
                dprintf(1, "Relocation %ld: offset=%ld, type=%ld, sym=%ld, addend=%ld\n",
                       j, rel->r_offset, r_type, ELF64_R_SYM(rel->r_info), rel->r_addend);
                
                /* 获取符号的值 */
                uint64_t sym_value;
                if (sym->st_shndx == SHN_UNDEF) {
                    /* 对于未定义的符号,使用0作为目标地址 */
                    dprintf(1, "Warning: GOT relocation for undefined symbol at 0x%lx\n", 
                           (unsigned long)((char*)info->obj_base + rel->r_offset));
                    sym_value = 0;
                } else {
                    /* 计算符号的实际地址 */
                    sym_value = (uint64_t)info->obj_base + sym->st_value;
                }
                
                /* 获取重定位的目标地址 */
                void* target = (char*)info->obj_base + rel->r_offset;
                dprintf(1, "Symbol %s at offset %p\n", sym_name, target);
                
                /* 根据重定位类型进行处理 */
                switch (r_type) {
                    case R_X86_64_64:   /* 直接64位 */
                        *(uint64_t*)target = sym_value + rel->r_addend;
                        dprintf(1, "R_X86_64_64: target=%p, value=%lx\n", 
                               target, sym_value + rel->r_addend);
                        break;
                        
                    case R_X86_64_32:   /* 32位绝对 */
                        *(uint32_t*)target = (uint32_t)(sym_value + rel->r_addend);
                        dprintf(1, "R_X86_64_32: target=%p, value=%lx\n",
                               target, (uint32_t)(sym_value + rel->r_addend));
                        break;
                        
                    case R_X86_64_PC32: /* 32位相对 */
                        *(uint32_t*)target = (uint32_t)(sym_value + rel->r_addend - 
                                                      (uint64_t)target);
                        dprintf(1, "R_X86_64_PC32: target=%p, value=%lx\n",
                               target, (uint32_t)(sym_value + rel->r_addend - (uint64_t)target));
                        break;
                        
                    case R_X86_64_PLT32:/* 同PC32 */
                        *(uint32_t*)target = (uint32_t)(sym_value + rel->r_addend - 
                                                      (uint64_t)target);
                        dprintf(1, "R_X86_64_PLT32: target=%p, value=%lx\n",
                               target, (uint32_t)(sym_value + rel->r_addend - (uint64_t)target));
                        break;
                        
                    case R_X86_64_GOTPCREL:
                    case R_X86_64_GOTPCRELX:
                    case R_X86_64_REX_GOTPCRELX:
                        {
                            /* 获取符号名 */
                            const char* sym_name = strtab + sym->st_name;
                            
                            /* 获取或创建GOT表项 */
                            struct got_entry* entry = get_got_entry(info, sym_name, 
                                                                  sym->st_shndx == SHN_UNDEF);
                            if (!entry) {
                                dprintf(2, "Failed to get GOT entry for symbol %s\n", sym_name);
                                return -1;
                            }
                            
                            if (sym->st_shndx == SHN_UNDEF) {
                                /* 对于未定义的符号，GOT表项已经在get_got_entry中设置为PLT存根 */
                                dprintf(1, "GOT entry for undefined symbol %s at %p (PLT at %p)\n", 
                                       sym_name, entry->got_addr, entry->plt_entry);
                            } else {
                                /* 对于已定义的符号，将符号地址写入GOT表项 */
                                entry->sym_addr = (void*)sym_value;
                                *(void**)entry->got_addr = entry->sym_addr;
                                dprintf(1, "GOT entry for defined symbol %s at %p points to %p\n",
                                       sym_name, entry->got_addr, entry->sym_addr);
                            }
                            
                            /* 计算GOT表项的相对偏移 */
                            *(uint32_t*)target = (uint32_t)((uint64_t)entry->got_addr + rel->r_addend - 
                                                          (uint64_t)target);
                            dprintf(1, "R_X86_64_GOTPCREL: target=0x%lx, got_addr=%p, offset=%x\n",
                                   (uint64_t)target, entry->got_addr, *(uint32_t*)target);
                        }
                        break;
                        
                    default:
                        dprintf(2, "Unknown relocation type: %ld\n", r_type);
                        return -1;
                }
            }
        }
    }
    
    return 0;
}

/* 显示用法信息 */
static void show_usage(const char* prog_name) {
    dprintf(2, "Usage: %s <lib_path> [func_name]\n", prog_name);
    dprintf(2, "  lib_path   Path to the static library file to load\n");
    dprintf(2, "  func_name  Name of the function to call (default: dl_main)\n");
    dprintf(2, "\nLife cycle functions:\n");
    dprintf(2, "  dl_init    Called before main function (if exists)\n");
    dprintf(2, "  dl_main    Default main function\n");
    dprintf(2, "  dl_fini    Called after main function (if exists)\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        show_usage(argv[0]);
        return 1;
    }

    const char* lib_path = argv[1];
    const char* func_name = argc >= 3 ? argv[2] : "dl_main";  /* 默认调用dl_main */
    
    /* 检查文件是否存在 */
    if (access(lib_path, F_OK) != 0) {
        dprintf(2, "Error: %s does not exist\n", lib_path);
        return 1;
    }
    
    /* 加载静态库 */
    struct load_info* info = load_lib(lib_path);
    if (!info || !info->obj_base) {
        return 1;  /* load_lib已经输出了错误信息 */
    }

    /* 检查ELF头 */
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)info->obj_base;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        dprintf(2, "Invalid ELF header in %s\n", lib_path);
        munmap(info->base, info->size);
        return 1;
    }
    dprintf(1, "ELF header magic: %02x %02x %02x %02x\n",
           ehdr->e_ident[0], ehdr->e_ident[1], ehdr->e_ident[2], ehdr->e_ident[3]);
    dprintf(1, "Section header table at offset %ld\n", ehdr->e_shoff);

    /* 处理重定位 */
    if (process_relocs(info) != 0) {
        dprintf(2, "Failed to process relocations\n");
        munmap(info->base, info->size);
        return 1;
    }

    /* 调用dl_init初始化 */
    void* init_func = find_symbol(info, "dl_init");
    if (init_func) {
        int (*dl_init_func)(void) = (int (*)(void))init_func;
        int init_result = dl_init_func();
        if (init_result != 0) {
            dprintf(2, "dl_init() failed with code %d\n", init_result);
            munmap(info->base, info->size);
            return 1;
        }
        dprintf(1, "dl_init() succeeded\n");
    }
    
    /* 查找并调用主函数 */
    void* func = find_symbol(info, func_name);
    if (!func) {
        munmap(info->base, info->size);
        return 1;  /* find_symbol已经输出了错误信息 */
    }
    
    /* 调用函数并获取返回值 */
    int (*main_func)(void) = (int (*)(void))func;
    dprintf(1, "Calling %s at %p\n", func_name, func);
    int result = main_func();
    dprintf(1, "%s() returned %d\n", func_name, result);

    /* 调用dl_fini清理 */
    void* fini_func = find_symbol(info, "dl_fini");
    if (fini_func) {
        int (*dl_fini_func)(void) = (int (*)(void))fini_func;
        int fini_result = dl_fini_func();
        if (fini_result != 0) {
            dprintf(2, "dl_fini() failed with code %d\n", fini_result);
        } else {
            dprintf(1, "dl_fini() succeeded\n");
        }
    }
    
    /* 卸载库 */
    dprintf(1, "%s has been unloaded\n", lib_path);
    munmap(info->base, info->size);
    free(info->got_table);
    return result;
} 