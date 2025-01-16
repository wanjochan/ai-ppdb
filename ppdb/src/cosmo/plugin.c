#include "cosmopolitan.h"
#include "plugin.h"

plugin_t* load_plugin(const char* path) {
    plugin_t* p = malloc(sizeof(plugin_t));
    if (!p) {
        printf("Failed to allocate plugin structure\n");
        return NULL;
    }

    // 打开插件文件
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open plugin file: %s\n", path);
        free(p);
        return NULL;
    }

    // 获取文件大小
    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    printf("Plugin file size: %ld bytes\n", size);

    // 映射插件到内存
    void* base = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE, fd, 0);
    close(fd);

    if (base == MAP_FAILED) {
        printf("Failed to map plugin file into memory\n");
        free(p);
        return NULL;
    }

    p->base = base;
    p->size = size;

    // 获取ELF头
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)base;
    
    // 检查ELF魔数
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        printf("Invalid ELF magic number\n");
        munmap(base, size);
        free(p);
        return NULL;
    }
    
    printf("ELF header:\n");
    printf("  Type: 0x%x\n", ehdr->e_type);
    printf("  Machine: 0x%x\n", ehdr->e_machine);
    printf("  Version: 0x%x\n", ehdr->e_version);
    printf("  Entry point: 0x%lx\n", ehdr->e_entry);
    printf("  Program header offset: 0x%lx\n", ehdr->e_phoff);
    printf("  Section header offset: 0x%lx\n", ehdr->e_shoff);
    
    // 检查是否是可执行文件
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        printf("Not an executable or shared object file\n");
        munmap(base, size);
        free(p);
        return NULL;
    }

    // 获取程序头表
    Elf64_Phdr* phdr = (Elf64_Phdr*)((char*)base + ehdr->e_phoff);
    
    // 遍历程序头表，找到代码段
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD && (phdr[i].p_flags & PF_X)) {
            // 找到可执行段，设置主函数指针
            p->main = (plugin_main_fn)((char*)base + phdr[i].p_vaddr);
            break;
        }
    }

    if (!p->main) {
        printf("Failed to locate main function\n");
        munmap(base, size);
        free(p);
        return NULL;
    }

    printf("Plugin loaded at %p, size %ld\n", base, size);
    printf("Main function at %p\n", p->main);
    printf("Entry point offset: 0x%lx\n", ehdr->e_entry);

    return p;
}

void unload_plugin(plugin_t* p) {
    if (p) {
        if (p->base) {
            munmap(p->base, p->size);
        }
        free(p);
    }
} 