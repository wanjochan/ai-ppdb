#include "cosmopolitan.h"

#define PAGE_SIZE 4096
#define ALIGN_DOWN(x, align) ((x) & ~((align)-1))
#define ALIGN_UP(x, align) ALIGN_DOWN((x) + (align)-1, (align))

#ifndef MEM_RESERVE
#define MEM_RESERVE 0x00002000
#endif

#ifndef MEM_COMMIT
#define MEM_COMMIT 0x00001000
#endif

#ifndef MEM_RELEASE
#define MEM_RELEASE 0x00008000
#endif

#ifndef PAGE_EXECUTE_READ
#define PAGE_EXECUTE_READ 0x20
#endif

#ifndef PAGE_EXECUTE_READWRITE
#define PAGE_EXECUTE_READWRITE 0x40
#endif

typedef int (*module_main_t)(void);

void *load_section(void *base, Elf64_Shdr *section, void *module_base) {
    if (section->sh_size == 0) {
        return NULL;
    }

    // Calculate the target virtual address
    void *target = (void *)((uintptr_t)base + section->sh_addr);
    
    // Copy section data
    memcpy(target, (char *)module_base + section->sh_offset, section->sh_size);
    
    printf("Loaded section to %p (size: %lu)\n", target, section->sh_size);
    return target;
}

// Forward declaration
int main(int argc, char *argv[]);

// Entry point
void _start(void) {
    exit(main(0, NULL));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <module.elf>\n", argv[0]);
        return 1;
    }

    const char *module_path = argv[1];
    printf("Loading module: %s\n", module_path);

    // Open module file
    int fd = open(module_path, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open module\n");
        return 1;
    }

    // Get file size
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    printf("Module size: %lu bytes\n", file_size);

    // Map module into memory
    void *module_base = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (module_base == MAP_FAILED) {
        printf("Failed to map module\n");
        close(fd);
        return 1;
    }

    // Verify ELF header
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)module_base;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        printf("Invalid ELF header\n");
        munmap(module_base, file_size);
        close(fd);
        return 1;
    }
    printf("ELF header verified\n");

    // Find section headers
    Elf64_Shdr *shdrs = (Elf64_Shdr *)((char *)module_base + ehdr->e_shoff);
    
    // Calculate maximum virtual address
    uintptr_t max_vaddr = 0;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *shdr = &shdrs[i];
        uintptr_t section_end = shdr->sh_addr + shdr->sh_size;
        if (section_end > max_vaddr) {
            max_vaddr = section_end;
        }
    }

    // Calculate load address and size
    void *preferred_base = (void *)0x110000000;  // Use a higher base address
    size_t total_size = ALIGN_UP(max_vaddr, PAGE_SIZE);

    // Map module at preferred address with executable permission
    void *mapped_base = VirtualAlloc(preferred_base, total_size,
                                   MEM_RESERVE | MEM_COMMIT,
                                   PAGE_EXECUTE_READWRITE);
    if (!mapped_base) {
        printf("Failed to allocate memory at %p (error: %u)\n",
               preferred_base, GetLastError());
        munmap(module_base, file_size);
        close(fd);
        return 1;
    }
    printf("Mapped module at: %p\n", mapped_base);

    // Load sections
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *shdr = &shdrs[i];
        
        // Get section name
        char *shstrtab = (char *)module_base + shdrs[ehdr->e_shstrndx].sh_offset;
        char *section_name = shstrtab + shdr->sh_name;
        
        printf("Section %d: %s at offset 0x%lx, addr 0x%lx\n",
               i, section_name, shdr->sh_offset, shdr->sh_addr);

        // Load section
        if (shdr->sh_type == SHT_PROGBITS || shdr->sh_type == SHT_NOBITS) {
            load_section(mapped_base, shdr, module_base);
        }
    }

    // Find module_main symbol
    Elf64_Shdr *symtab = NULL;
    Elf64_Shdr *strtab = NULL;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            symtab = &shdrs[i];
            strtab = &shdrs[shdrs[i].sh_link];
            break;
        }
    }

    void *module_main = NULL;
    if (symtab && strtab) {
        Elf64_Sym *syms = (Elf64_Sym *)((char *)module_base + symtab->sh_offset);
        char *strs = (char *)module_base + strtab->sh_offset;
        
        for (int i = 0; i < symtab->sh_size / sizeof(Elf64_Sym); i++) {
            if (strcmp(strs + syms[i].st_name, "module_main") == 0) {
                module_main = (void *)((uintptr_t)mapped_base + syms[i].st_value);
                printf("Found module_main at virtual address %p\n", module_main);
                break;
            }
        }
    }

    if (!module_main) {
        printf("Failed to find module_main symbol\n");
        VirtualFree(mapped_base, 0, MEM_RELEASE);
        munmap(module_base, file_size);
        close(fd);
        return 1;
    }

    // Call module_main
    printf("Calling module_main at %p\n", module_main);
    module_main_t entry = (module_main_t)module_main;
    int result = entry();
    printf("Module returned: %d\n", result);

    // Cleanup
    VirtualFree(mapped_base, 0, MEM_RELEASE);
    munmap(module_base, file_size);
    close(fd);

    return result;
}
