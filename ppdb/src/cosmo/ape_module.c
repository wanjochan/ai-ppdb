#include "plugin.h"
#include "ape_module.h"

// 查找段
static void* find_section(void* base, const char* name, size_t* size) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)base;
    Elf64_Shdr* shdr = (Elf64_Shdr*)((char*)base + ehdr->e_shoff);
    char* shstrtab = (char*)base + shdr[ehdr->e_shstrndx].sh_offset;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (strcmp(shstrtab + shdr[i].sh_name, name) == 0) {
            if (size) *size = shdr[i].sh_size;
            return (char*)base + shdr[i].sh_offset;
        }
    }
    return NULL;
}

// 处理重定位
static void apply_relocations(ape_module_t* mod) {
    for (size_t i = 0; i < mod->rela_count; i++) {
        Elf64_Rela* rel = &mod->rela[i];
        Elf64_Sym* sym = &mod->symtab[ELF64_R_SYM(rel->r_info)];
        void* target = (char*)mod->base + rel->r_offset;
        
        switch (ELF64_R_TYPE(rel->r_info)) {
            case R_X86_64_64:   // 直接64位
                *(uint64_t*)target = (uint64_t)mod->base + sym->st_value + rel->r_addend;
                break;
                
            case R_X86_64_PC32: // PC相对32位
            case R_X86_64_PLT32:
                *(uint32_t*)target = (uint32_t)((uint64_t)mod->base + sym->st_value + 
                    rel->r_addend - (uint64_t)target);
                break;
                
            case R_X86_64_GOTPCREL: // GOT表项
                // 简化处理:直接使用符号地址
                *(uint32_t*)target = (uint32_t)((uint64_t)mod->base + sym->st_value + 
                    rel->r_addend - (uint64_t)target);
                break;
        }
    }
}

ape_module_t* load_ape_module(const char* path) {
    ape_module_t* mod = malloc(sizeof(ape_module_t));
    if (!mod) return NULL;
    memset(mod, 0, sizeof(ape_module_t));
    
    // 打开并映射文件
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        free(mod);
        return NULL;
    }
    
    mod->size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    
    mod->base = mmap(NULL, mod->size, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (mod->base == MAP_FAILED) {
        free(mod);
        return NULL;
    }
    
    // 验证ELF头
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)mod->base;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        unload_ape_module(mod);
        return NULL;
    }
    
    // 定位关键段
    size_t symtab_size = 0;
    mod->symtab = find_section(mod->base, ".symtab", &symtab_size);
    mod->sym_count = symtab_size / sizeof(Elf64_Sym);
    
    mod->strtab = find_section(mod->base, ".strtab", NULL);
    
    size_t rela_size = 0;
    mod->rela = find_section(mod->base, ".rela.text", &rela_size);
    mod->rela_count = rela_size / sizeof(Elf64_Rela);
    
    // 设置入口点
    mod->entry = (char*)mod->base + ehdr->e_entry;
    
    // 处理重定位
    apply_relocations(mod);
    
    return mod;
}

void* find_symbol(ape_module_t* mod, const char* name) {
    for (size_t i = 0; i < mod->sym_count; i++) {
        const char* sym_name = mod->strtab + mod->symtab[i].st_name;
        if (strcmp(sym_name, name) == 0) {
            return (char*)mod->base + mod->symtab[i].st_value;
        }
    }
    return NULL;
}

void unload_ape_module(ape_module_t* mod) {
    if (mod) {
        if (mod->base) {
            munmap(mod->base, mod->size);
        }
        free(mod);
    }
}

/* 加载插件 */
plugin_t* load_plugin(const char* path) {
    /* 加载APE模块 */
    ape_module_t* mod = load_ape_module(path);
    if (!mod) {
        return NULL;
    }

    /* 创建插件结构 */
    plugin_t* p = malloc(sizeof(plugin_t));
    if (!p) {
        unload_ape_module(mod);
        return NULL;
    }

    /* 初始化插件结构 */
    p->base = mod->base;
    p->size = mod->size;
    p->main = (plugin_main_fn)find_symbol(mod, "_dl_main");

    /* 如果找不到主函数,释放资源并返回NULL */
    if (!p->main) {
        free(p);
        unload_ape_module(mod);
        return NULL;
    }

    /* 打印调试信息 */
    printf("Module loaded at %p\n", mod->base);
    printf("Module size: %zu bytes\n", mod->size);
    printf("Entry point: %p\n", mod->entry);
    printf("Symbol count: %zu\n", mod->sym_count);
    printf("Found _dl_main at %p\n", p->main);

    return p;
}

/* 卸载插件 */
void unload_plugin(plugin_t* p) {
    if (p) {
        /* 创建临时APE模块结构 */
        ape_module_t mod = {
            .base = p->base,
            .size = p->size
        };
        /* 卸载APE模块 */
        unload_ape_module(&mod);
        /* 释放插件结构 */
        free(p);
    }
}