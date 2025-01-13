#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

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

    return base;
}

/* 查找符号 */
static void* find_symbol(void* base, const char* name) {
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)base;
    const Elf64_Shdr* shdr = (const Elf64_Shdr*)((char*)base + ehdr->e_shoff);

    /* 查找符号表和字符串表 */
    const Elf64_Shdr* symtab = NULL;
    const char* strtab = NULL;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            symtab = &shdr[i];
            strtab = (char*)base + shdr[shdr[i].sh_link].sh_offset;
            break;
        }
    }

    if (!symtab || !strtab) {
        dprintf(2, "Symbol table not found\n");
        return NULL;
    }

    /* 在符号表中查找 */
    const Elf64_Sym* syms = (const Elf64_Sym*)((char*)base + symtab->sh_offset);
    int num_syms = symtab->sh_size / sizeof(Elf64_Sym);

    for (int i = 0; i < num_syms; i++) {
        const char* sym_name = strtab + syms[i].st_name;
        if (strcmp(sym_name, name) == 0) {
            return (char*)base + syms[i].st_value;
        }
    }

    return NULL;
}

int main(void) {
    char libpath[2048];
    const char* libname = "test4.dl";
    
    /* 获取当前工作目录 */
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        dprintf(1, "Current working directory: %s\n", cwd);
        /* 构建完整路径 */
        snprintf(libpath, sizeof(libpath), "%s/%s", cwd, libname);
    } else {
        dprintf(2, "Failed to get current directory\n");
        return 1;
    }
    
    /* 检查文件是否存在 */
    if (access(libpath, F_OK) != 0) {
        dprintf(2, "Error: %s does not exist\n", libpath);
        return 1;
    }
    
    dprintf(1, "File %s exists, attempting to load...\n", libpath);
    
    /* 加载 DL */
    void* base = load_dl(libpath);
    if (!base) {
        dprintf(2, "Failed to load %s\n", libpath);
        return 1;
    }
    
    dprintf(1, "Successfully loaded %s at %p\n", libpath, base);
    
    /* 获取并调用导出函数 */
    int (*test4_func)(void) = find_symbol(base, "test4_func");
    if (test4_func) {
        int result = test4_func();
        dprintf(1, "test4_func() returned: %d\n", result);
    } else {
        dprintf(2, "Failed to get test4_func\n");
        munmap(base, 0);  /* 大小参数在这里不重要 */
        return 1;
    }
    
    /* 卸载 DL */
    munmap(base, 0);  /* 大小参数在这里不重要 */
    dprintf(1, "%s unloaded\n", libpath);
    
    return 0;
} 