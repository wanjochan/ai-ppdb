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

/* 查找符号 */
static void* find_symbol(void* base, const char* name) {
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)base;
    
    /* 查找动态段 */
    const Elf64_Phdr* phdr = (const Elf64_Phdr*)((char*)base + ehdr->e_phoff);
    const Elf64_Dyn* dyn = NULL;
    
    dprintf(1, "ELF header at %p, program headers at %p\n", ehdr, phdr);
    dprintf(1, "Number of program headers: %d\n", ehdr->e_phnum);
    
    for (int i = 0; i < ehdr->e_phnum; i++) {
        dprintf(1, "Program header %d: type=%x, offset=%lx, vaddr=%lx, paddr=%lx, filesz=%lx, memsz=%lx, flags=%x, align=%lx\n",
                i, phdr[i].p_type, phdr[i].p_offset, phdr[i].p_vaddr, phdr[i].p_paddr,
                phdr[i].p_filesz, phdr[i].p_memsz, phdr[i].p_flags, phdr[i].p_align);
        if (phdr[i].p_type == PT_DYNAMIC) {
            dyn = (const Elf64_Dyn*)((char*)base + phdr[i].p_offset);
            break;
        }
    }
    
    if (!dyn) {
        dprintf(2, "Dynamic section not found\n");
        return NULL;
    }
    
    dprintf(1, "Dynamic section found at %p\n", dyn);
    
    /* 从动态段获取必要信息 */
    const Elf64_Sym* dynsym = NULL;
    const char* dynstr = NULL;
    const Elf64_Word* hash = NULL;
    Elf64_Addr symtab_size = 0;
    
    for (; dyn->d_tag != DT_NULL; dyn++) {
        dprintf(1, "Dynamic entry: tag=%lx, val=%lx\n", dyn->d_tag, dyn->d_un.d_val);
        switch (dyn->d_tag) {
            case DT_SYMTAB:
                dynsym = (const Elf64_Sym*)((char*)base + dyn->d_un.d_ptr);
                dprintf(1, "Symbol table found at %p\n", dynsym);
                break;
            case DT_STRTAB:
                dynstr = (const char*)base + dyn->d_un.d_ptr;
                dprintf(1, "String table found at %p\n", dynstr);
                break;
            case DT_HASH:
                hash = (const Elf64_Word*)((char*)base + dyn->d_un.d_ptr);
                symtab_size = hash[1]; /* nchain */
                dprintf(1, "Hash table found at %p, symbol count: %d\n", hash, symtab_size);
                break;
        }
    }
    
    if (!dynsym || !dynstr) {
        dprintf(2, "Symbol information not found\n");
        return NULL;
    }
    
    /* 在动态符号表中查找 */
    for (Elf64_Word i = 0; i < symtab_size; i++) {
        const char* sym_name = dynstr + dynsym[i].st_name;
        dprintf(1, "Symbol %d: name=%s, value=%lx, size=%lx, info=%x, other=%x, shndx=%d\n",
                i, sym_name, dynsym[i].st_value, dynsym[i].st_size,
                dynsym[i].st_info, dynsym[i].st_other, dynsym[i].st_shndx);
        if (strcmp(sym_name, name) == 0) {
            dprintf(1, "Found symbol %s at offset %lx\n", name, dynsym[i].st_value);
            return (char*)base + dynsym[i].st_value;
        }
    }
    
    dprintf(2, "Symbol %s not found\n", name);
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
        dprintf(1, "Found test4_func at %p\n", test4_func);
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