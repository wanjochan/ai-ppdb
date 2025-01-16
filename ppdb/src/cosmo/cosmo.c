#include "cosmopolitan.h"

/* 包装函数 */
void* ape_stack_round(void* p) {
    return p;
}

/* 插件头部魔数和版本 */
#define PLUGIN_MAGIC 0x50504442
#define PLUGIN_VERSION 1

/* APE头部魔数 */
#define APE_MZ_MAGIC 0x5A4D
#define APE_PE_MAGIC 0x4550
#define APE_ELF_MAGIC 0x464C457F
#define APE_MACHO_MAGIC 0xFEEDFACF

/* 插件头部结构 */
struct plugin_header {
    uint32_t magic;
    uint32_t version;
    uint32_t init_offset;
    uint32_t main_offset;
    uint32_t fini_offset;
};

/* APE头部结构 */
struct ape_header {
    uint64_t mz_magic;      /* DOS MZ 魔数 */
    uint8_t  pad1[0x3c];
    uint32_t pe_magic;      /* PE 魔数 */
    uint16_t machine;       /* AMD64 */
    uint16_t num_sections;
    uint32_t timestamp;
    uint8_t  pad2[0x40];
    uint32_t elf_magic;     /* ELF 魔数 */
    uint8_t  elf_class;     /* 64位 */
    uint8_t  elf_data;      /* 小端 */
    uint8_t  elf_version;   /* 版本1 */
    uint8_t  elf_abi;       /* System V */
    uint64_t elf_pad;
    uint16_t elf_type;      /* ET_DYN */
    uint16_t elf_machine;   /* x86-64 */
    uint32_t elf_version2;  /* 版本1 */
    uint64_t elf_entry;     /* 入口点 */
    uint8_t  pad3[0x40];
    uint32_t macho_magic;   /* Mach-O 魔数 */
    uint32_t macho_cputype;
    uint32_t macho_cpusubtype;
    uint32_t macho_filetype;
    uint32_t macho_ncmds;
    uint32_t macho_sizeofcmds;
    uint32_t macho_flags;
    uint32_t macho_reserved;
};

/* 验证APE头部 */
static bool verify_ape(void* base) {
    struct ape_header* header = (struct ape_header*)base;
    printf("Verifying APE header:\n");
    printf("  MZ magic: 0x%llx\n", header->mz_magic);
    printf("  PE magic: 0x%x\n", header->pe_magic);
    printf("  ELF magic: 0x%x\n", header->elf_magic);
    printf("  Mach-O magic: 0x%x\n", header->macho_magic);
    printf("  Entry point: 0x%llx\n", header->elf_entry);

    if (header->mz_magic != APE_MZ_MAGIC || 
        header->pe_magic != APE_PE_MAGIC ||
        header->elf_magic != APE_ELF_MAGIC ||
        header->macho_magic != APE_MACHO_MAGIC) {
        printf("Not an APE file\n");
        return false;
    }

    return true;
}

/* 加载插件 */
static void* load_plugin(const char* path, size_t* size) {
    printf("Loading plugin: %s\n", path);
    
    /* 打开插件文件 */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open plugin: %s\n", path);
        return NULL;
    }

    /* 获取文件大小 */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Failed to stat plugin\n");
        close(fd);
        return NULL;
    }
    *size = st.st_size;
    printf("Plugin file size: %zu bytes\n", *size);

    /* 映射文件到内存 */
    void* base = mmap(NULL, st.st_size, 
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE, fd, 0);
    close(fd);

    if (base == MAP_FAILED) {
        printf("Failed to mmap plugin\n");
        return NULL;
    }
    printf("Plugin mapped at: %p\n", base);

    return base;
}

/* 验证插件 */
static bool verify_plugin(void* base) {
    struct plugin_header* header = (struct plugin_header*)base;
    printf("Verifying plugin header:\n");
    printf("  Magic: 0x%x\n", header->magic);
    printf("  Version: %d\n", header->version);
    printf("  Init offset: 0x%x\n", header->init_offset);
    printf("  Main offset: 0x%x\n", header->main_offset);
    printf("  Fini offset: 0x%x\n", header->fini_offset);
    
    if (header->magic != PLUGIN_MAGIC) {
        printf("Invalid plugin magic: expected 0x%x, got 0x%x\n",
               PLUGIN_MAGIC, header->magic);
        return false;
    }

    if (header->version != PLUGIN_VERSION) {
        printf("Invalid plugin version: expected %d, got %d\n",
               PLUGIN_VERSION, header->version);
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    /* 初始化stdio */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* 检查参数 */
    if (argc != 2) {
        printf("Usage: %s <plugin.dl|program.exe>\n", argv[0]);
        return 1;
    }

    size_t size;
    void* base = load_plugin(argv[1], &size);
    if (!base) {
        return 1;
    }

    /* 尝试作为插件加载 */
    if (verify_plugin(base)) {
        struct plugin_header* header = (struct plugin_header*)base;
        
        /* 获取函数指针 */
        int (*init)(void) = (int (*)(void))((char*)base + header->init_offset);
        int (*main_func)(void) = (int (*)(void))((char*)base + header->main_offset);
        int (*fini)(void) = (int (*)(void))((char*)base + header->fini_offset);

        printf("Function addresses:\n");
        printf("  init: %p (offset: 0x%x)\n", init, header->init_offset);
        printf("  main: %p (offset: 0x%x)\n", main_func, header->main_offset);
        printf("  fini: %p (offset: 0x%x)\n", fini, header->fini_offset);

        /* 执行插件 */
        int ret = 0;
        if (init) {
            printf("Calling init...\n");
            ret = init();
            if (ret != 0) {
                printf("Plugin init failed: %d\n", ret);
                goto cleanup;
            }
            printf("Init returned: %d\n", ret);
        }

        if (main_func) {
            printf("Calling main...\n");
            ret = main_func();
            printf("Main returned: %d\n", ret);
        }

        if (fini) {
            printf("Calling fini...\n");
            ret = fini();
            printf("Fini returned: %d\n", ret);
        }

        goto cleanup;
    }

    /* 尝试作为APE程序加载 */
    if (verify_ape(base)) {
        struct ape_header* header = (struct ape_header*)base;
        
        /* 获取入口点 */
        int (*entry)(void) = (int (*)(void))((char*)base + header->elf_entry);
        printf("APE entry point: %p (offset: 0x%llx)\n", entry, header->elf_entry);

        /* 执行程序 */
        int ret = entry();
        printf("Program returned: %d\n", ret);
        goto cleanup;
    }

    printf("File is neither a valid plugin nor a valid APE program\n");

cleanup:
    munmap(base, size);
    return 0;
} 