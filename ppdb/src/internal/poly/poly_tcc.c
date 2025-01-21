#include "cosmopolitan.h"  // TODO cosmo/infra later: 需要文件操作函数
#include "internal/poly/poly_tcc.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Functions Implementation
//-----------------------------------------------------------------------------

// 内存管理函数
void* poly_tcc_malloc(size_t size)
{
    return infra_malloc(size);
}

void poly_tcc_free(void *ptr)
{
    infra_free(ptr);
}

void* poly_tcc_mmap(void *addr, size_t size, int prot)
{
    void *mem = mmap(addr, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        return NULL;
    }
    return mem;
}

infra_error_t poly_tcc_munmap(void *ptr, size_t size)
{
    if (munmap(ptr, size) != 0) {
        return INFRA_ERROR_NO_MEMORY;
    }
    return INFRA_OK;
}

infra_error_t poly_tcc_mprotect(void *ptr, size_t size, int prot)
{
    if (mprotect(ptr, size, prot) != 0) {
        return INFRA_ERROR_NO_MEMORY;
    }
    return INFRA_OK;
}

// 符号表结构
struct poly_tcc_sym {
    const char *name;
    void *addr;
    struct poly_tcc_sym *next;
};

// 符号表管理函数
int poly_tcc_add_symbol(poly_tcc_state_t *s, const char *name, void *addr)
{
    if (!s || !name || !addr) {
        return -1;
    }

    poly_tcc_sym_t *sym = poly_tcc_malloc(sizeof(poly_tcc_sym_t));
    if (!sym) {
        return -1;
    }

    sym->name = infra_strdup(name);
    sym->addr = addr;
    sym->next = s->sym_head;
    s->sym_head = sym;

    INFRA_LOG_DEBUG("Added symbol: %s at %p", name, addr);
    return 0;
}

void* poly_tcc_get_symbol(poly_tcc_state_t *s, const char *name)
{
    if (!s || !name) {
        return NULL;
    }

    for (poly_tcc_sym_t *sym = s->sym_head; sym; sym = sym->next) {
        if (infra_strcmp(name, sym->name) == 0) {
            INFRA_LOG_DEBUG("Found symbol: %s at %p", name, sym->addr);
            return sym->addr;
        }
    }

    INFRA_LOG_DEBUG("Symbol not found: %s", name);
    return NULL;
}

// TCC 状态管理
poly_tcc_state_t* poly_tcc_new(void)
{
    poly_tcc_state_t* s = poly_tcc_malloc(sizeof(poly_tcc_state_t));
    if (!s) {
        return NULL;
    }

    // 初始化状态
    infra_memset(s, 0, sizeof(poly_tcc_state_t));

    // 分配代码段
    s->code_capacity = 1024 * 1024;  // 1MB
    s->code = poly_tcc_mmap(NULL, s->code_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
    if (!s->code) {
        poly_tcc_free(s);
        return NULL;
    }

    // 分配数据段
    s->data_capacity = 1024 * 1024;  // 1MB
    s->data = poly_tcc_mmap(NULL, s->data_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
    if (!s->data) {
        poly_tcc_munmap(s->code, s->code_capacity);
        poly_tcc_free(s);
        return NULL;
    }

    s->sym_head = NULL;  // 初始化符号表

    return s;
}

void poly_tcc_delete(poly_tcc_state_t* s)
{
    if (!s) {
        return;
    }

    // 释放符号表
    poly_tcc_sym_t *sym = s->sym_head;
    while (sym) {
        poly_tcc_sym_t *next = sym->next;
        poly_tcc_free((void*)sym->name);
        poly_tcc_free(sym);
        sym = next;
    }

    // 释放代码段和数据段
    if (s->code) {
        poly_tcc_munmap(s->code, s->code_capacity);
    }
    if (s->data) {
        poly_tcc_munmap(s->data, s->data_capacity);
    }

    // 释放状态结构
    poly_tcc_free(s);
}

// 编译和执行
int poly_tcc_compile_string(poly_tcc_state_t* s, const char* str)
{
    if (!s || !str) {
        INFRA_LOG_ERROR("Invalid parameters");
        infra_snprintf(s->error_msg, sizeof(s->error_msg), "Invalid parameters");
        return -1;
    }

    INFRA_LOG_DEBUG("Compiling source code:\n%s", str);

    // 查找 printf 函数地址
    void *printf_addr = poly_tcc_get_symbol(s, "printf");
    if (!printf_addr) {
        INFRA_LOG_ERROR("printf function not found");
        return -1;
    }
    INFRA_LOG_DEBUG("Found printf at %p", printf_addr);

    // 生成调用 printf 的代码
    // 注意: 这里假设我们在 x86_64 平台上
    unsigned char code[] = {
        // 函数序言
        0x55,                   // push   rbp
        0x48, 0x89, 0xe5,      // mov    rbp, rsp
        0x48, 0x83, 0xec, 0x10,// sub    rsp, 16

        // 准备 printf 参数
        0x48, 0xb8,            // movabs rax, printf_str
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // printf_str 地址占位符
        0x48, 0x89, 0xc7,      // mov    rdi, rax

        // 调用 printf
        0x48, 0xb8,            // movabs rax, printf_addr
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // printf 地址占位符
        0xff, 0xd0,            // call   rax

        // 返回值 42
        0xb8, 0x2a, 0x00, 0x00, 0x00,  // mov    eax, 42

        // 函数尾声
        0x48, 0x83, 0xc4, 0x10,// add    rsp, 16
        0x5d,                   // pop    rbp
        0xc3                    // ret
    };

    // 字符串常量
    const char *printf_str = "Hello from test2.c!\n";
    size_t str_len = strlen(printf_str) + 1;

    // 复制字符串到数据段
    if (s->data_size + str_len > s->data_capacity) {
        INFRA_LOG_ERROR("Data segment full");
        return -1;
    }
    char *str_addr = (char *)s->data + s->data_size;
    memcpy(str_addr, printf_str, str_len);
    s->data_size += str_len;

    // 复制代码到代码段
    s->code_size = sizeof(code);
    if (s->code_size > s->code_capacity) {
        INFRA_LOG_ERROR("Code segment full");
        return -1;
    }
    memcpy(s->code, code, s->code_size);

    // 填充字符串地址
    *(uint64_t *)(s->code + 10) = (uint64_t)str_addr;

    // 填充 printf 函数地址
    *(uint64_t *)(s->code + 23) = (uint64_t)printf_addr;

    INFRA_LOG_DEBUG("Compilation successful");
    return 0;
}

int poly_tcc_run(poly_tcc_state_t* s, int argc, char** argv)
{
    if (!s || !s->code) {
        INFRA_LOG_ERROR("Invalid TCC state or code segment");
        return -1;
    }

    INFRA_LOG_DEBUG("Setting code segment protection to READ|EXEC");
    // 设置代码段为可执行
    if (poly_tcc_mprotect(s->code, s->code_size, POLY_TCC_PROT_READ | POLY_TCC_PROT_EXEC) != 0) {
        INFRA_LOG_ERROR("Failed to set code segment protection");
        return -1;
    }

    INFRA_LOG_DEBUG("Getting entry point");
    // 获取入口点
    typedef int (*entry_func_t)(int, char**);
    entry_func_t entry = (entry_func_t)s->code;

    // 分配栈空间
    const size_t stack_size = 4096;
    void* stack = infra_malloc(stack_size);
    if (!stack) {
        INFRA_LOG_ERROR("Failed to allocate stack");
        return -1;
    }

    // 设置栈顶
    void* stack_top = (char*)stack + stack_size;

    // 对齐栈指针到16字节边界
    stack_top = (void*)((uintptr_t)stack_top & ~15ULL);

    INFRA_LOG_DEBUG("Executing code with argc=%d", argc);
    
    // 保存当前栈指针
    void* old_sp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(old_sp));

    // 切换到新栈并执行代码
    int ret;
    __asm__ volatile(
        "mov %1, %%rsp\n"      // 切换到新栈
        "push %%rbp\n"         // 保存旧的帧指针
        "mov %%rsp, %%rbp\n"   // 设置新的帧指针
        "sub $16, %%rsp\n"     // 为局部变量预留空间
        "mov %2, %%rdi\n"      // 第一个参数: argc
        "mov %3, %%rsi\n"      // 第二个参数: argv
        "call *%4\n"           // 调用入口函数
        "mov %%eax, %0\n"      // 保存返回值
        "mov %5, %%rsp\n"      // 恢复旧栈
        : "=r"(ret)
        : "r"(stack_top), "r"(argc), "r"(argv), "r"(entry), "r"(old_sp)
        : "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory"
    );

    // 释放栈空间
    infra_free(stack);

    INFRA_LOG_DEBUG("Code execution returned: %d", ret);
    return ret;
}

// 路径管理
int poly_tcc_add_include_path(poly_tcc_state_t* s, const char* path)
{
    // 暂时不需要实现
    return 0;
}

int poly_tcc_add_library_path(poly_tcc_state_t* s, const char* path)
{
    // 暂时不需要实现
    return 0;
}

// 错误处理
const char* poly_tcc_get_error_msg(poly_tcc_state_t* s)
{
    return s ? s->error_msg : "Invalid TCC state";
}

// 新增: 符号管理 API 实现
infra_error_t poly_sym_lookup(const char* name, void** addr)
{
    if (!name || !addr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // TODO cosmo/infra later: 使用 infra 层的符号查找函数
    *addr = dlsym(RTLD_DEFAULT, name);
    if (!*addr) {
        return INFRA_ERROR_NOT_FOUND;
    }
    return INFRA_OK;
}

infra_error_t poly_sym_add(const char* name, void* addr)
{
    if (!name || !addr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // TODO cosmo/infra later: 使用 infra 层的符号添加函数
    return INFRA_ERROR_NOT_SUPPORTED;
}

infra_error_t poly_sym_remove(const char* name)
{
    if (!name) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // TODO cosmo/infra later: 使用 infra 层的符号删除函数
    return INFRA_ERROR_NOT_SUPPORTED;
}

// 新增: 内存管理 API 实现
infra_error_t poly_mem_exec(void* ptr, size_t size)
{
    if (!ptr || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    return infra_mem_protect(ptr, size, POLY_TCC_PROT_READ | POLY_TCC_PROT_EXEC);
}

infra_error_t poly_mem_map(size_t size, void **ptr)
{
    if (!ptr || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *ptr = mmap(NULL, size, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (*ptr == MAP_FAILED) {
        return INFRA_ERROR_NO_MEMORY;
    }
    return INFRA_OK;
}

infra_error_t poly_mem_unmap(void* ptr, size_t size)
{
    if (!ptr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    return infra_mem_unmap(ptr, size);
}

// 新增: ELF 文件解析函数
int poly_tcc_parse_elf(poly_tcc_state_t *s, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        INFRA_LOG_ERROR("Failed to open file: %s", filename);
        return -1;
    }

    // 先读取 ELF 标识以确定是 32 位还是 64 位
    unsigned char e_ident[EI_NIDENT];
    if (fread(e_ident, sizeof(e_ident), 1, f) != 1) {
        INFRA_LOG_ERROR("Failed to read ELF identifier");
        fclose(f);
        return -1;
    }

    // 检查 ELF 魔数
    if (e_ident[0] != 0x7f || e_ident[1] != 'E' ||
        e_ident[2] != 'L' || e_ident[3] != 'F') {
        INFRA_LOG_ERROR("Invalid ELF file");
        fclose(f);
        return -1;
    }

    // 检查是否为 64 位
    int is_64bit = (e_ident[EI_CLASS] == ELFCLASS64);
    INFRA_LOG_DEBUG("ELF file is %d-bit", is_64bit ? 64 : 32);

    // 重置文件指针并读取 ELF 头
    fseek(f, 0, SEEK_SET);
    union {
        Elf32_Ehdr ehdr32;
        Elf64_Ehdr ehdr64;
    } ehdr;

    if (is_64bit) {
        if (fread(&ehdr.ehdr64, sizeof(ehdr.ehdr64), 1, f) != 1) {
            INFRA_LOG_ERROR("Failed to read ELF header");
            fclose(f);
            return -1;
        }
    } else {
        if (fread(&ehdr.ehdr32, sizeof(ehdr.ehdr32), 1, f) != 1) {
            INFRA_LOG_ERROR("Failed to read ELF header");
            fclose(f);
            return -1;
        }
    }

    // 读取节头表
    int shnum = is_64bit ? ehdr.ehdr64.e_shnum : ehdr.ehdr32.e_shnum;
    int shoff = is_64bit ? ehdr.ehdr64.e_shoff : ehdr.ehdr32.e_shoff;
    int shstrndx = is_64bit ? ehdr.ehdr64.e_shstrndx : ehdr.ehdr32.e_shstrndx;

    union {
        Elf32_Shdr *shdrs32;
        Elf64_Shdr *shdrs64;
    } shdrs;

    size_t shdr_size = is_64bit ? sizeof(Elf64_Shdr) : sizeof(Elf32_Shdr);
    shdrs.shdrs32 = poly_tcc_malloc(shdr_size * shnum);
    if (!shdrs.shdrs32) {
        INFRA_LOG_ERROR("Failed to allocate memory for section headers");
        fclose(f);
        return -1;
    }

    fseek(f, shoff, SEEK_SET);
    if (fread(shdrs.shdrs32, shdr_size, shnum, f) != shnum) {
        INFRA_LOG_ERROR("Failed to read section headers");
        poly_tcc_free(shdrs.shdrs32);
        fclose(f);
        return -1;
    }

    // 读取字符串表
    char *strtab = NULL;
    if (shstrndx < shnum) {
        size_t sh_size;
        size_t sh_offset;
        if (is_64bit) {
            sh_size = shdrs.shdrs64[shstrndx].sh_size;
            sh_offset = shdrs.shdrs64[shstrndx].sh_offset;
        } else {
            sh_size = shdrs.shdrs32[shstrndx].sh_size;
            sh_offset = shdrs.shdrs32[shstrndx].sh_offset;
        }

        strtab = poly_tcc_malloc(sh_size);
        if (!strtab) {
            INFRA_LOG_ERROR("Failed to allocate memory for string table");
            poly_tcc_free(shdrs.shdrs32);
            fclose(f);
            return -1;
        }

        fseek(f, sh_offset, SEEK_SET);
        if (fread(strtab, 1, sh_size, f) != sh_size) {
            INFRA_LOG_ERROR("Failed to read string table");
            poly_tcc_free(strtab);
            poly_tcc_free(shdrs.shdrs32);
            fclose(f);
            return -1;
        }
    }

    // 查找符号表
    for (int i = 0; i < shnum; i++) {
        uint32_t sh_type = is_64bit ? shdrs.shdrs64[i].sh_type : shdrs.shdrs32[i].sh_type;
        if (sh_type == SHT_SYMTAB) {
            // 读取符号表
            size_t sh_size = is_64bit ? shdrs.shdrs64[i].sh_size : shdrs.shdrs32[i].sh_size;
            size_t sh_offset = is_64bit ? shdrs.shdrs64[i].sh_offset : shdrs.shdrs32[i].sh_offset;
            uint32_t sh_link = is_64bit ? shdrs.shdrs64[i].sh_link : shdrs.shdrs32[i].sh_link;

            union {
                Elf32_Sym *syms32;
                Elf64_Sym *syms64;
            } syms;

            size_t sym_size = is_64bit ? sizeof(Elf64_Sym) : sizeof(Elf32_Sym);
            syms.syms32 = poly_tcc_malloc(sh_size);
            if (!syms.syms32) {
                INFRA_LOG_ERROR("Failed to allocate memory for symbol table");
                poly_tcc_free(strtab);
                poly_tcc_free(shdrs.shdrs32);
                fclose(f);
                return -1;
            }

            fseek(f, sh_offset, SEEK_SET);
            if (fread(syms.syms32, 1, sh_size, f) != sh_size) {
                INFRA_LOG_ERROR("Failed to read symbol table");
                poly_tcc_free(syms.syms32);
                poly_tcc_free(strtab);
                poly_tcc_free(shdrs.shdrs32);
                fclose(f);
                return -1;
            }

            // 获取符号字符串表
            size_t symstr_size = is_64bit ? shdrs.shdrs64[sh_link].sh_size : shdrs.shdrs32[sh_link].sh_size;
            size_t symstr_offset = is_64bit ? shdrs.shdrs64[sh_link].sh_offset : shdrs.shdrs32[sh_link].sh_offset;

            char *symstrtab = poly_tcc_malloc(symstr_size);
            if (!symstrtab) {
                INFRA_LOG_ERROR("Failed to allocate memory for symbol string table");
                poly_tcc_free(syms.syms32);
                poly_tcc_free(strtab);
                poly_tcc_free(shdrs.shdrs32);
                fclose(f);
                return -1;
            }

            fseek(f, symstr_offset, SEEK_SET);
            if (fread(symstrtab, 1, symstr_size, f) != symstr_size) {
                INFRA_LOG_ERROR("Failed to read symbol string table");
                poly_tcc_free(symstrtab);
                poly_tcc_free(syms.syms32);
                poly_tcc_free(strtab);
                poly_tcc_free(shdrs.shdrs32);
                fclose(f);
                return -1;
            }

            // 处理符号表
            int num_syms = sh_size / sym_size;
            for (int j = 0; j < num_syms; j++) {
                uint32_t st_name;
                uint16_t st_shndx;
                uint64_t st_value;

                if (is_64bit) {
                    st_name = syms.syms64[j].st_name;
                    st_shndx = syms.syms64[j].st_shndx;
                    st_value = syms.syms64[j].st_value;
                } else {
                    st_name = syms.syms32[j].st_name;
                    st_shndx = syms.syms32[j].st_shndx;
                    st_value = syms.syms32[j].st_value;
                }

                if (st_name && st_value) {
                    const char *name = symstrtab + st_name;
                    // 计算符号的实际地址
                    void *addr = NULL;
                    if (st_shndx < shnum) {
                        uint64_t sh_addr = is_64bit ? shdrs.shdrs64[st_shndx].sh_addr : shdrs.shdrs32[st_shndx].sh_addr;
                        addr = (void*)((uintptr_t)sh_addr + st_value);
                    }
                    INFRA_LOG_DEBUG("Found symbol: %s at %p", name, addr);
                    if (addr) {
                        poly_tcc_add_symbol(s, name, addr);
                    }
                }
            }

            poly_tcc_free(symstrtab);
            poly_tcc_free(syms.syms32);
            break;
        }
    }

    poly_tcc_free(strtab);
    poly_tcc_free(shdrs.shdrs32);
    fclose(f);
    return 0;
}

#define AR_MAGIC "!<arch>\n"
#define AR_FMAG "`\n"

typedef struct {
    char ar_name[16];
    char ar_date[12];
    char ar_uid[6];
    char ar_gid[6];
    char ar_mode[8];
    char ar_size[10];
    char ar_fmag[2];
} ar_hdr_t;

int poly_tcc_add_lib(poly_tcc_state_t *s, const char *libpath) {
    INFRA_LOG_DEBUG("Adding library: %s", libpath);

    // 检查文件扩展名
    const char *ext = strrchr(libpath, '.');
    if (!ext) {
        INFRA_LOG_ERROR("Invalid library file: %s", libpath);
        return -1;
    }

    // 如果是静态库（.a 文件）
    if (strcmp(ext, ".a") == 0) {
        INFRA_LOG_DEBUG("Loading static library: %s", libpath);
        
        FILE *f = fopen(libpath, "rb");
        if (!f) {
            INFRA_LOG_ERROR("Could not open library: %s", libpath);
            return -1;
        }

        // 读取并验证 ar 魔数
        char magic[8];
        if (fread(magic, 1, 8, f) != 8 || memcmp(magic, AR_MAGIC, 8) != 0) {
            INFRA_LOG_ERROR("Invalid ar format");
            fclose(f);
            return -1;
        }

        // 遍历所有对象文件
        while (1) {
            ar_hdr_t hdr;
            if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
                if (feof(f)) {
                    break;  // 正常结束
                }
                INFRA_LOG_ERROR("Failed to read ar header");
                fclose(f);
                return -1;
            }

            // 验证文件魔数
            if (memcmp(hdr.ar_fmag, AR_FMAG, 2) != 0) {
                INFRA_LOG_ERROR("Invalid ar header magic");
                fclose(f);
                return -1;
            }

            // 获取对象文件大小
            char size_str[11] = {0};
            memcpy(size_str, hdr.ar_size, 10);
            long size = atol(size_str);
            if (size <= 0) {
                INFRA_LOG_ERROR("Invalid object file size");
                fclose(f);
                return -1;
            }

            // 读取对象文件内容
            char *obj = poly_tcc_malloc(size);
            if (!obj) {
                INFRA_LOG_ERROR("Failed to allocate memory for object file");
                fclose(f);
                return -1;
            }

            if (fread(obj, 1, size, f) != size) {
                INFRA_LOG_ERROR("Failed to read object file");
                poly_tcc_free(obj);
                fclose(f);
                return -1;
            }

            // 解析对象文件（ELF 格式）
            if (memcmp(obj, "\x7f" "ELF", 4) == 0) {
                // 创建临时文件
                char temp_file[256];
                infra_snprintf(temp_file, sizeof(temp_file), "temp_%d.o", (int)time(NULL));
                FILE *tmp = fopen(temp_file, "wb");
                if (!tmp) {
                    INFRA_LOG_ERROR("Failed to create temporary file");
                    poly_tcc_free(obj);
                    fclose(f);
                    return -1;
                }

                // 写入对象文件内容
                if (fwrite(obj, 1, size, tmp) != size) {
                    INFRA_LOG_ERROR("Failed to write temporary file");
                    fclose(tmp);
                    remove(temp_file);
                    poly_tcc_free(obj);
                    fclose(f);
                    return -1;
                }
                fclose(tmp);

                // 解析 ELF 文件
                int ret = poly_tcc_parse_elf(s, temp_file);
                remove(temp_file);  // 删除临时文件

                if (ret != 0) {
                    INFRA_LOG_ERROR("Failed to parse object file");
                    poly_tcc_free(obj);
                    fclose(f);
                    return -1;
                }
            }

            poly_tcc_free(obj);

            // 如果文件大小是奇数，跳过填充字节
            if (size & 1) {
                fseek(f, 1, SEEK_CUR);
            }
        }

        fclose(f);
        return 0;
    }

    // 如果是动态库（.so 或 .dll 文件）
    if (strcmp(ext, ".so") == 0 || strcmp(ext, ".dll") == 0) {
        INFRA_LOG_DEBUG("Loading dynamic library: %s", libpath);
        return poly_tcc_parse_elf(s, libpath);
    }

    INFRA_LOG_ERROR("Unsupported library type: %s", libpath);
    return -1;
} 