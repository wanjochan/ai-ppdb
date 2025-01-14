#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

/* APE头大小 */
#define APE_HEADER_SIZE 4096

/* 加载 DL 文件 */
static void* load_dl(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        dprintf(2, "Failed to open %s\n", path);
        return NULL;
    }

    /* 获取文件大小 */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        dprintf(2, "Failed to stat %s\n", path);
        close(fd);
        return NULL;
    }

    /* 映射文件 */
    void* base = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE, fd, 0);
    close(fd);

    if (base == MAP_FAILED) {
        dprintf(2, "Failed to mmap %s\n", path);
        return NULL;
    }

    /* 检查APE头 */
    uint64_t* header = (uint64_t*)base;
    if (header[0] == 0x13371337) {
        dprintf(1, "APE header found, skipping %d bytes\n", APE_HEADER_SIZE);
        return (char*)base + APE_HEADER_SIZE;
    }

    return base;
}

/* 处理重定位 */
static int process_relocs(void* base) {
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)base;
    
    /* 验证 ELF 头 */
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        dprintf(2, "Invalid ELF header\n");
        return -1;
    }
    
    /* 查找节头表 */
    const Elf64_Shdr* shdr = (const Elf64_Shdr*)((char*)base + ehdr->e_shoff);
    
    /* 查找符号表和字符串表 */
    const Elf64_Sym* symtab = NULL;
    const char* strtab = NULL;
    size_t symcount = 0;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            symtab = (const Elf64_Sym*)((char*)base + shdr[i].sh_offset);
            symcount = shdr[i].sh_size / sizeof(Elf64_Sym);
            
            /* 字符串表在符号表之后的节中 */
            if (i + 1 < ehdr->e_shnum) {
                strtab = (const char*)base + shdr[i + 1].sh_offset;
            }
            break;
        }
    }
    
    if (!symtab || !strtab) {
        dprintf(2, "Symbol or string table not found\n");
        return -1;
    }
    
    /* 查找重定位节 */
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_RELA) {
            const Elf64_Rela* rela = (const Elf64_Rela*)((char*)base + shdr[i].sh_offset);
            size_t num_relocs = shdr[i].sh_size / sizeof(Elf64_Rela);
            
            /* 处理每个重定位项 */
            for (size_t j = 0; j < num_relocs; j++) {
                Elf64_Addr* target = (Elf64_Addr*)((char*)base + rela[j].r_offset);
                uint64_t sym_value = 0;
                uint32_t sym_index = ELF64_R_SYM(rela[j].r_info);
                
                /* 获取符号值 */
                if (sym_index > 0 && sym_index < symcount) {
                    const Elf64_Sym* sym = &symtab[sym_index];
                    if (sym->st_shndx != SHN_UNDEF) {
                        sym_value = (uint64_t)base + sym->st_value;
                        dprintf(1, "Symbol %s at offset %lx\n", strtab + sym->st_name, sym_value);
                    }
                }
                
                switch (ELF64_R_TYPE(rela[j].r_info)) {
                    case R_X86_64_NONE:
                        break;
                        
                    case R_X86_64_64:    /* 类型1: 直接64位 */
                        *target = sym_value + rela[j].r_addend;
                        break;
                        
                    case R_X86_64_PC32:  /* 类型2: 32位PC相对 */
                        *(uint32_t*)target = (uint32_t)(sym_value + rela[j].r_addend - (uint64_t)target);
                        break;
                        
                    case R_X86_64_32:    /* 类型10: 32位绝对 */
                        *(uint32_t*)target = (uint32_t)(sym_value + rela[j].r_addend);
                        break;
                        
                    case R_X86_64_32S:   /* 类型11: 有符号32位 */
                        *(int32_t*)target = (int32_t)(sym_value + rela[j].r_addend);
                        break;
                        
                    case R_X86_64_PLT32: /* 类型4: PLT项32位PC相对 */
                        *(uint32_t*)target = (uint32_t)(sym_value + rela[j].r_addend - (uint64_t)target);
                        break;
                        
                    case R_X86_64_RELATIVE:  /* 类型8: 基址相对 */
                        *target = (Elf64_Addr)base + rela[j].r_addend;
                        break;
                        
                    case R_X86_64_GOTPCREL:    /* 类型37: GOT项PC相对 */
                    case R_X86_64_GOTPCRELX:   /* 类型41: 优化的GOT加载 */
                    case R_X86_64_REX_GOTPCRELX: /* 类型42: 带REX前缀的优化GOT加载 */
                        /* 直接使用符号地址，不使用GOT */
                        if (sym_value) {
                            *(uint32_t*)target = (uint32_t)(sym_value + rela[j].r_addend - (uint64_t)target);
                            dprintf(1, "GOT relocation at %p: target=%p, value=%lx\n", target, (void*)(sym_value + rela[j].r_addend), sym_value);
                        } else {
                            dprintf(2, "Warning: GOT relocation for undefined symbol at %p\n", target);
                        }
                        break;
                        
                    default:
                        dprintf(2, "Unsupported relocation type: %ld\n", ELF64_R_TYPE(rela[j].r_info));
                        break;
                }
            }
        }
    }
    
    return 0;
}

/* 查找符号 */
static void* find_symbol(void* base, const char* name) {
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)base;
    
    /* 验证 ELF 头 */
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        dprintf(2, "Invalid ELF header\n");
        return NULL;
    }
    
    /* 查找节头表 */
    const Elf64_Shdr* shdr = (const Elf64_Shdr*)((char*)base + ehdr->e_shoff);
    
    /* 查找符号表和字符串表 */
    const Elf64_Sym* symtab = NULL;
    const char* strtab = NULL;
    size_t symcount = 0;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            symtab = (const Elf64_Sym*)((char*)base + shdr[i].sh_offset);
            symcount = shdr[i].sh_size / sizeof(Elf64_Sym);
            
            /* 字符串表在符号表之后的节中 */
            if (i + 1 < ehdr->e_shnum) {
                strtab = (const char*)base + shdr[i + 1].sh_offset;
            }
            break;
        }
    }
    
    if (!symtab || !strtab) {
        dprintf(2, "Symbol or string table not found\n");
        return NULL;
    }
    
    /* 在符号表中查找 */
    for (size_t i = 0; i < symcount; i++) {
        const char* sym_name = strtab + symtab[i].st_name;
        if (strcmp(sym_name, name) == 0) {
            /* 检查符号类型和绑定 */
            unsigned char type = ELF64_ST_TYPE(symtab[i].st_info);
            unsigned char bind = ELF64_ST_BIND(symtab[i].st_info);
            
            /* 只返回函数或对象符号，且必须是全局或弱绑定 */
            if ((type == STT_FUNC || type == STT_OBJECT) &&
                (bind == STB_GLOBAL || bind == STB_WEAK)) {
                dprintf(1, "Found symbol %s at offset %lx (type=%d, bind=%d)\n", 
                       name, symtab[i].st_value, type, bind);
                return (char*)base + symtab[i].st_value;
            }
        }
    }
    
    dprintf(2, "Symbol %s not found\n", name);
    return NULL;
}

/* 显示用法信息 */
static void show_usage(const char* prog_name) {
    dprintf(2, "Usage: %s <dl_path> <func_name> [args...]\n", prog_name);
    dprintf(2, "  dl_path    Path to the DL file to load\n");
    dprintf(2, "  func_name  Name of the function to call\n");
    dprintf(2, "  args       Optional arguments to pass to the function\n");
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        show_usage(argv[0]);
        return 1;
    }

    const char* dl_path = argv[1];
    const char* func_name = argv[2];
    
    /* 检查文件是否存在 */
    if (access(dl_path, F_OK) != 0) {
        dprintf(2, "Error: %s does not exist\n", dl_path);
        return 1;
    }
    
    /* 加载 DL */
    void* base = load_dl(dl_path);
    if (!base) {
        dprintf(2, "Failed to load %s\n", dl_path);
        return 1;
    }
    
    /* 处理重定位 */
    if (process_relocs(base) != 0) {
        dprintf(2, "Failed to process relocations\n");
        munmap(base, 0);
        return 1;
    }
    
    /* 查找并调用函数 */
    void* func = find_symbol(base, func_name);
    if (!func) {
        dprintf(2, "Function %s not found\n", func_name);
        munmap(base, 0);
        return 1;
    }
    
    /* 根据函数名判断返回类型 */
    if (strstr(func_name, "_str")) {
        /* 字符串返回值 */
        const char* (*str_func)(void) = (const char* (*)(void))func;
        dprintf(1, "Found %s at %p\n", func_name, func);
        const char* result = str_func();
        dprintf(1, "%s\n", result);
    } else if (strstr(func_name, "_print") || strstr(func_name, "_set_msg") || strstr(func_name, "_free")) {
        /* void返回值 */
        void (*void_func)(void) = (void (*)(void))func;
        dprintf(1, "Found %s at %p\n", func_name, func);
        dprintf(1, "Calling void function %s\n", func_name);
        void_func();
        dprintf(1, "Completed void function %s\n", func_name);
    } else {
        /* 整数返回值 */
        int (*int_func)(void) = (int (*)(void))func;
        dprintf(1, "Found %s at %p\n", func_name, func);
        int result = int_func();
        dprintf(1, "%s() returned %d\n", func_name, result);
    }
    
    /* 卸载DL */
    dprintf(1, "%s has been unloaded\n", dl_path);
    munmap(base, 0);
    return 0;
} 