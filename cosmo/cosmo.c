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

static void *map_memory(size_t size, DWORD protect) {
    void *ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, protect);
    if (!ptr) {
        printf("Failed to allocate memory (size: %zu)\n", size);
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
    if (ehdr->e_type != 1) {  // relocatable
        printf("Not a relocatable ELF file\n");
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

    // First pass: find .text section
    Elf64_Shdr *shdr = (Elf64_Shdr*)(data + ehdr->e_shoff);
    const char *shstrtab = (const char*)(data + shdr[ehdr->e_shstrndx].sh_offset);
    Elf64_Shdr *text_shdr = NULL;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char *name = get_string(shstrtab, shdr[i].sh_name);
        printf("Section %d: %s at offset 0x%lx, addr 0x%lx, size 0x%lx, align 0x%lx\n",
               i, name, shdr[i].sh_offset, shdr[i].sh_addr, shdr[i].sh_size, shdr[i].sh_addralign);

        if (shdr[i].sh_type == 1 && shdr[i].sh_size > 0) {  // PROGBITS
            if (strcmp(name, ".text") == 0) {
                text_shdr = &shdr[i];
                break;
            }
        }
    }

    if (!text_shdr) {
        printf(".text section not found\n");
        free(data);
        return 1;
    }

    // Allocate memory for code
    size_t text_size = ROUND_UP(text_shdr->sh_size, text_shdr->sh_addralign);
    void *code_base = map_memory(text_size, PAGE_READWRITE);
    if (!code_base) {
        free(data);
        return 1;
    }
    printf("Mapped code at: %p (size: %zu)\n", code_base, text_size);

    // Load .text section
    memcpy(code_base, data + text_shdr->sh_offset, text_shdr->sh_size);
    printf("Loaded .text section to %p (size: %zu)\n", code_base, (size_t)text_shdr->sh_size);

    // Find module_main symbol
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
        unmap_memory(code_base, text_size);
        free(data);
        return 1;
    }

    Elf64_Sym *syms = (Elf64_Sym*)(data + symtab->sh_offset);
    int num_syms = symtab->sh_size / symtab->sh_entsize;
    module_main_t module_main = NULL;

    for (int i = 0; i < num_syms; i++) {
        const char *name = get_string(strtab, syms[i].st_name);
        if (strcmp(name, "module_main") == 0) {
            module_main = (module_main_t)((char*)code_base + syms[i].st_value);
            printf("Found module_main at offset 0x%lx\n", syms[i].st_value);
            break;
        }
    }

    if (!module_main) {
        printf("module_main symbol not found\n");
        unmap_memory(code_base, text_size);
        free(data);
        return 1;
    }

    // Change memory protection to execute-only
    DWORD old_protect;
    if (!VirtualProtect(code_base, text_size, PAGE_EXECUTE_READ, &old_protect)) {
        printf("Failed to change memory protection\n");
        unmap_memory(code_base, text_size);
        free(data);
        return 1;
    }

    // Flush instruction cache
    FlushInstructionCache(GetCurrentProcess(), code_base, text_size);

    // Print code bytes
    printf("Code bytes at %p: ", module_main);
    for (int i = 0; i < 16 && i < text_size; i++) {
        printf("%02x ", ((uint8_t*)module_main)[i]);
    }
    printf("\n");

    printf("Calling module_main at %p\n", module_main);
    int result = module_main();
    printf("module_main returned %d\n", result);

    unmap_memory(code_base, text_size);
    free(data);
    return 0;
}
