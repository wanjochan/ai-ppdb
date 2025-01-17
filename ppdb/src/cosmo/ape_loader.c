#include "cosmopolitan.h"
#include "ape_loader.h"

#define READ32(x) (*(uint32_t*)(x))
#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PF_R 4

typedef struct {
    void* base;
    size_t size;
    Elf64_Ehdr* ehdr;
    Elf64_Phdr* phdr;
    Elf64_Shdr* shdr;
    char* strtab;
    Elf64_Sym* symtab;
    size_t symcount;
} loaded_module_t;

static void* find_elf_header(void* base, size_t size) {
    unsigned char* p = (unsigned char*)base;
    
    // 验证 MZ 签名 (0x4d5a)
    if (p[0] != 0x4d || p[1] != 0x5a) {
        printf("Invalid MZ signature: %02x %02x\n", p[0], p[1]);
        return NULL;
    }
    printf("MZ signature verified\n");
    
    // 获取 PE 头部偏移
    uint32_t pe_offset = *(uint32_t*)(p + 0x3c);
    if (pe_offset >= size - 4) {
        printf("Invalid PE header offset: 0x%x\n", pe_offset);
        return NULL;
    }
    
    // 验证 PE 签名 ("PE\0\0")
    if (READ32(p + pe_offset) != 0x00004550) {
        printf("Invalid PE signature at 0x%x: %08x\n", pe_offset, READ32(p + pe_offset));
        return NULL;
    }
    printf("PE signature verified at 0x%x\n", pe_offset);
    
    // 从 PE 头部开始搜索 ELF 魔数
    unsigned char* search_start = p + pe_offset;
    unsigned char* search_end = p + size - 16;
    unsigned char* ptr = search_start;
    
    printf("Searching for ELF header from 0x%zx to 0x%zx\n", 
           search_start - p, search_end - p);
    
    while (ptr < search_end) {
        // 检查 ELF 魔数
        if (READ32(ptr) == 0x464C457F) { // "\x7FELF"
            // 验证其他 ELF 头部字段
            Elf64_Ehdr* ehdr = (Elf64_Ehdr*)ptr;
            if (ehdr->e_ident[EI_CLASS] == ELFCLASS64 &&
                ehdr->e_ident[EI_DATA] == ELFDATA2LSB &&
                ehdr->e_ident[EI_VERSION] == EV_CURRENT &&
                ehdr->e_type <= ET_DYN &&
                ehdr->e_machine == EM_X86_64) {
                printf("Found valid ELF header at offset 0x%zx\n", ptr - p);
                printf("  Class: %d (ELFCLASS64)\n", ehdr->e_ident[EI_CLASS]);
                printf("  Data: %d (ELFDATA2LSB)\n", ehdr->e_ident[EI_DATA]);
                printf("  Version: %d\n", ehdr->e_ident[EI_VERSION]);
                printf("  Type: 0x%x\n", ehdr->e_type);
                printf("  Machine: 0x%x (EM_X86_64)\n", ehdr->e_machine);
                printf("  Entry point: 0x%lx\n", ehdr->e_entry);
                printf("  Program header offset: 0x%lx\n", ehdr->e_phoff);
                printf("  Section header offset: 0x%lx\n", ehdr->e_shoff);
                return ehdr;
            }
        }
        // 按页面大小对齐增加搜索指针
        ptr += 0x1000;
    }
    
    printf("ELF header not found in range 0x%zx - 0x%zx\n", 
           search_start - p, search_end - p);
    return NULL;
}

static bool load_symbols(loaded_module_t* module) {
    Elf64_Ehdr* ehdr = module->ehdr;
    
    // 获取节表
    module->shdr = (Elf64_Shdr*)((char*)ehdr + ehdr->e_shoff);
    
    // 获取字符串表
    char* shstrtab = (char*)ehdr + module->shdr[ehdr->e_shstrndx].sh_offset;
    
    // 查找符号表和字符串表
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr* sh = &module->shdr[i];
        char* name = shstrtab + sh->sh_name;
        
        if (sh->sh_type == SHT_SYMTAB) {
            module->symtab = (Elf64_Sym*)((char*)ehdr + sh->sh_offset);
            module->symcount = sh->sh_size / sizeof(Elf64_Sym);
            
            // 获取关联的字符串表
            module->strtab = (char*)ehdr + module->shdr[sh->sh_link].sh_offset;
            printf("Found symbol table with %zu entries\n", module->symcount);
            return true;
        }
    }
    
    return false;
}

void* ape_load(const char* filename) {
    printf("Opening file: %s\n", filename);
    
    // 打开文件
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open file: %s\n", strerror(errno));
        return NULL;
    }
    
    // 获取文件大小
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Failed to get file size: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    printf("File size: %ld bytes\n", st.st_size);
    
    // 映射整个文件
    void* base = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) {
        printf("Failed to map file: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    
    // 查找 ELF 头部
    Elf64_Ehdr* ehdr = find_elf_header(base, st.st_size);
    if (!ehdr) {
        munmap(base, st.st_size);
        close(fd);
        return NULL;
    }
    
    // 验证 ELF 头部
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
        ehdr->e_ident[EI_DATA] != ELFDATA2LSB ||
        ehdr->e_ident[EI_VERSION] != EV_CURRENT ||
        ehdr->e_type > ET_DYN ||
        ehdr->e_machine != EM_X86_64) {
        printf("Invalid ELF header fields:\n");
        printf("  Class: %d\n", ehdr->e_ident[EI_CLASS]);
        printf("  Data: %d\n", ehdr->e_ident[EI_DATA]);
        printf("  Version: %d\n", ehdr->e_ident[EI_VERSION]);
        printf("  Type: 0x%x\n", ehdr->e_type);
        printf("  Machine: 0x%x\n", ehdr->e_machine);
        munmap(base, st.st_size);
        close(fd);
        return NULL;
    }
    printf("ELF header verified\n");
    
    // 创建模块信息
    loaded_module_t* module = calloc(1, sizeof(loaded_module_t));
    if (!module) {
        munmap(base, st.st_size);
        close(fd);
        return NULL;
    }
    
    module->base = base;
    module->size = st.st_size;
    module->ehdr = ehdr;
    module->phdr = (Elf64_Phdr*)((char*)ehdr + ehdr->e_phoff);
    
    // 加载符号表
    if (!load_symbols(module)) {
        printf("Warning: No symbol table found\n");
    }
    
    // 遍历程序头部，设置内存权限
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr* ph = &module->phdr[i];
        if (ph->p_type == PT_LOAD) {
            void* addr = (void*)((char*)ehdr + ph->p_offset);
            size_t size = ph->p_memsz;
            size_t file_size = ph->p_filesz;
            int prot = 0;
            
            // 设置内存保护
            if (ph->p_flags & PF_R) prot |= PROT_READ;
            if (ph->p_flags & PF_W) prot |= PROT_WRITE;
            if (ph->p_flags & PF_X) prot |= PROT_EXEC;
            
            // 对齐到页面大小
            size_t page_offset = (size_t)addr & 0xfff;
            addr = (void*)((size_t)addr & ~0xfff);
            size = (size + page_offset + 0xfff) & ~0xfff;
            
            printf("Setting segment protection:\n");
            printf("  Address: %p\n", addr);
            printf("  Size: 0x%zx\n", size);
            printf("  Protection: %c%c%c\n",
                   (prot & PROT_READ) ? 'r' : '-',
                   (prot & PROT_WRITE) ? 'w' : '-',
                   (prot & PROT_EXEC) ? 'x' : '-');
            
            if (mprotect(addr, size, prot) < 0) {
                printf("Failed to set protection: %s\n", strerror(errno));
                ape_unload(module);
                return NULL;
            }
            
            // 初始化 bss 段
            if (file_size < ph->p_memsz) {
                memset((char*)addr + file_size, 0, ph->p_memsz - file_size);
                printf("Initialized BSS: %zu bytes\n", ph->p_memsz - file_size);
            }
        }
    }
    
    close(fd);
    return module;
}

void* ape_get_proc(void* handle, const char* symbol) {
    loaded_module_t* module = (loaded_module_t*)handle;
    if (!module || !module->symtab || !module->strtab) {
        return NULL;
    }
    
    printf("Looking for symbol: %s\n", symbol);
    
    // 遍历符号表查找符号
    for (size_t i = 0; i < module->symcount; i++) {
        Elf64_Sym* sym = &module->symtab[i];
        const char* name = module->strtab + sym->st_name;
        
        if (ELF64_ST_TYPE(sym->st_info) == STT_FUNC &&
            sym->st_value != 0 &&
            strcmp(name, symbol) == 0) {
            printf("Found symbol %s at offset 0x%lx\n", name, sym->st_value);
            return (char*)module->ehdr + sym->st_value;
        }
    }
    
    printf("Symbol not found: %s\n", symbol);
    return NULL;
}

void ape_unload(void* handle) {
    if (!handle) return;
    
    loaded_module_t* module = (loaded_module_t*)handle;
    if (module->base) {
        munmap(module->base, module->size);
    }
    free(module);
} 