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
    // Call main with no arguments for now
    exit(main(0, NULL));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <module.dat>\n", argv[0]);
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
    void *module_base = mmap(NULL, file_size, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0);
    if (module_base == MAP_FAILED) {
        printf("Failed to map module\n");
        close(fd);
        return 1;
    }

    // Call module_main
    printf("Calling module_main at %p\n", module_base);
    module_main_t entry = (module_main_t)module_base;
    int result = entry();
    printf("Module returned: %d\n", result);

    // Cleanup
    munmap(module_base, file_size);
    close(fd);

    return result;
}
