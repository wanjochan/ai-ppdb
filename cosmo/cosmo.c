#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>

#define PAGE_SIZE 4096
#define ROUND_UP(x, y) (((x) + (y) - 1) & ~((y) - 1))
#define ROUND_DOWN(x, y) ((x) & ~((y) - 1))

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

typedef struct {
    uint64_t r_offset;
    uint32_t r_type;
    uint32_t r_sym;
    uint64_t r_addend;
} Elf64_Rela;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

static void *map_memory(void *addr, size_t size, DWORD protect) {
    void *ptr = VirtualAlloc(addr, size, MEM_COMMIT | MEM_RESERVE, protect);
    if (!ptr) {
        printf("Failed to allocate memory at %p (size: %zu)\n", addr, size);
        return NULL;
    }
    return ptr;
}

static void unmap_memory(void *addr, size_t size) {
    VirtualFree(addr, 0, MEM_RELEASE);
}

static int verify_elf_header(const Elf64_Ehdr *ehdr) {
    if (memcmp(ehdr->e_ident, "\x7f" "ELF", 4) != 0) {
        printf("Not an ELF file\n");
        return 0;
    }
    if (ehdr->e_ident[4] != 2) {  // 64-bit
        printf("Not a 64-bit ELF file\n");
        return 0;
    }
    if (ehdr->e_ident[5] != 1) {  // little endian
        printf("Not a little-endian ELF file\n");
        return 0;
    }
    if (ehdr->e_type != 2) {  // executable
        printf("Not an executable ELF file\n");
        return 0;
    }
    if (ehdr->e_machine != 62) {  // x86_64
        printf("Not an x86_64 ELF file\n");
        return 0;
    }
    return 1;
}

static const char *get_string(const char *strtab, uint32_t offset) {
    return strtab + offset;
}

typedef int (*module_main_t)(void);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <module>\n", argv[0]);
        return 1;
    }

    const char *module_path = argv[1];
    FILE *f = fopen(module_path, "rb");
    if (!f) {
        printf("Failed to open module: %s\n", module_path);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    printf("Loading module: %s\n", module_path);
    printf("Module size: %zu bytes\n", size);

    uint8_t *data = malloc(size);
    if (!data) {
        printf("Failed to allocate memory for module\n");
        fclose(f);
        return 1;
    }

    if (fread(data, 1, size, f) != size) {
        printf("Failed to read module\n");
        free(data);
        fclose(f);
        return 1;
    }
    fclose(f);

    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)data;
    if (!verify_elf_header(ehdr)) {
        free(data);
        return 1;
    }
    printf("ELF header verified\n");

    // Load program headers
    Elf64_Phdr *phdr = (Elf64_Phdr*)(data + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != 1) {  // LOAD
            continue;
        }

        printf("Loading segment %d: vaddr=0x%lx, size=0x%lx, flags=0x%x\n", 
               i, phdr[i].p_vaddr, phdr[i].p_memsz, phdr[i].p_flags);

        // Calculate page-aligned addresses and sizes
        uintptr_t vaddr = ROUND_DOWN(phdr[i].p_vaddr, PAGE_SIZE);
        uintptr_t vaddr_end = ROUND_UP(phdr[i].p_vaddr + phdr[i].p_memsz, PAGE_SIZE);
        size_t map_size = vaddr_end - vaddr;

        // Map memory with appropriate protection
        DWORD protect = PAGE_READWRITE;
        if ((phdr[i].p_flags & 1) && (phdr[i].p_flags & 4)) {  // PF_X | PF_R
            protect = PAGE_EXECUTE_READ;
        } else if (phdr[i].p_flags & 4) {  // PF_R
            protect = PAGE_READONLY;
        }

        void *base = map_memory((void*)vaddr, map_size, PAGE_READWRITE);
        if (!base) {
            printf("Failed to map segment %d\n", i);
            free(data);
            return 1;
        }

        // Copy segment data
        if (phdr[i].p_filesz > 0) {
            printf("Copying segment data: offset=0x%lx, size=0x%lx\n", 
                   phdr[i].p_offset, phdr[i].p_filesz);
            memcpy((void*)(vaddr + (phdr[i].p_vaddr - vaddr)), 
                   data + phdr[i].p_offset, 
                   phdr[i].p_filesz);
        }

        // Initialize BSS if needed
        if (phdr[i].p_memsz > phdr[i].p_filesz) {
            printf("Initializing BSS: addr=0x%lx, size=0x%lx\n",
                   vaddr + (phdr[i].p_vaddr - vaddr) + phdr[i].p_filesz,
                   phdr[i].p_memsz - phdr[i].p_filesz);
            memset((void*)(vaddr + (phdr[i].p_vaddr - vaddr) + phdr[i].p_filesz),
                   0,
                   phdr[i].p_memsz - phdr[i].p_filesz);
        }

        // Change protection
        DWORD old_protect;
        if (!VirtualProtect(base, map_size, protect, &old_protect)) {
            printf("Failed to change memory protection for segment %d\n", i);
            unmap_memory(base, map_size);
            free(data);
            return 1;
        }

        printf("Loaded segment %d at %p (size: %zu)\n", i, base, map_size);
    }

    // Find module_main symbol
    Elf64_Shdr *shdr = (Elf64_Shdr*)(data + ehdr->e_shoff);
    Elf64_Shdr *symtab = NULL;
    const char *strtab = NULL;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == 2) {  // SYMTAB
            symtab = &shdr[i];
            strtab = (const char*)(data + shdr[shdr[i].sh_link].sh_offset);
            break;
        }
    }

    if (!symtab || !strtab) {
        printf("Symbol table not found\n");
        free(data);
        return 1;
    }

    Elf64_Sym *syms = (Elf64_Sym*)(data + symtab->sh_offset);
    int num_syms = symtab->sh_size / symtab->sh_entsize;
    module_main_t module_main = NULL;

    for (int i = 0; i < num_syms; i++) {
        const char *name = get_string(strtab, syms[i].st_name);
        if (strcmp(name, "module_main") == 0) {
            module_main = (module_main_t)syms[i].st_value;
            printf("Found module_main at virtual address 0x%lx\n", syms[i].st_value);
            break;
        }
    }

    if (!module_main) {
        printf("module_main symbol not found\n");
        free(data);
        return 1;
    }

    // Flush instruction cache
    FlushInstructionCache(GetCurrentProcess(), (void*)ROUND_DOWN((uintptr_t)module_main, PAGE_SIZE), PAGE_SIZE);

    printf("Calling module_main at %p\n", module_main);
    int result = module_main();
    printf("Module returned: %d\n", result);

    free(data);
    return 0;
}
