#ifndef POLY_TCC_H
#define POLY_TCC_H

//a tiny clone of tinycc, to make our ppdb to run c codes in JIT mode

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_error.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define POLY_TCC_MAX_PATH_LEN 256
#define POLY_TCC_MAX_SYMBOL_LEN 256

// 内存保护标志
#define POLY_TCC_PROT_NONE  0
#define POLY_TCC_PROT_READ  1
#define POLY_TCC_PROT_WRITE 2
#define POLY_TCC_PROT_EXEC  4

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// ELF 格式定义
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;

// ELF 常量定义
#define EI_NIDENT 16

// ELF 节类型
#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_HASH     5
#define SHT_DYNAMIC  6
#define SHT_NOTE     7
#define SHT_NOBITS   8
#define SHT_REL      9
#define SHT_SHLIB    10
#define SHT_DYNSYM   11

// ELF 符号绑定类型
#define STB_LOCAL   0
#define STB_GLOBAL  1
#define STB_WEAK    2

// ELF 符号类型
#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4

// ELF 头
typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;
    Elf32_Off  e_phoff;
    Elf32_Off  e_shoff;
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
} Elf32_Ehdr;

// 节头
typedef struct {
    Elf32_Word sh_name;
    Elf32_Word sh_type;
    Elf32_Word sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off  sh_offset;
    Elf32_Word sh_size;
    Elf32_Word sh_link;
    Elf32_Word sh_info;
    Elf32_Word sh_addralign;
    Elf32_Word sh_entsize;
} Elf32_Shdr;

// 符号表项
typedef struct {
    Elf32_Word st_name;
    Elf32_Addr st_value;
    Elf32_Word st_size;
    unsigned char st_info;
    unsigned char st_other;
    Elf32_Half st_shndx;
} Elf32_Sym;

// 前向声明
typedef struct poly_tcc_sym poly_tcc_sym_t;

// TCC 状态
typedef struct poly_tcc_state {
    // 代码段
    unsigned char* code;
    size_t code_size;
    size_t code_capacity;

    // 数据段
    unsigned char* data;
    size_t data_size;
    size_t data_capacity;

    // 符号表
    poly_tcc_sym_t *sym_head;

    // 错误信息
    char error_msg[256];
} poly_tcc_state_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// TCC 状态管理
poly_tcc_state_t* poly_tcc_new(void);
void poly_tcc_delete(poly_tcc_state_t *s);

// 编译和执行
int poly_tcc_compile_string(poly_tcc_state_t *s, const char *str);
int poly_tcc_run(poly_tcc_state_t *s, int argc, char **argv);

// 符号管理
int poly_tcc_add_symbol(poly_tcc_state_t *s, const char *name, void *addr);
void* poly_tcc_get_symbol(poly_tcc_state_t *s, const char *name);

// 错误处理
const char* poly_tcc_get_error_msg(poly_tcc_state_t *s);

// 内存管理
void* poly_tcc_malloc(size_t size);
void poly_tcc_free(void *ptr);

// 内存管理函数
void* poly_tcc_mmap(void *addr, size_t size, int prot);
infra_error_t poly_tcc_munmap(void *ptr, size_t size);
infra_error_t poly_tcc_mprotect(void *ptr, size_t size, int prot);

// 路径管理
int poly_tcc_add_include_path(poly_tcc_state_t* s, const char* path);
int poly_tcc_add_library_path(poly_tcc_state_t* s, const char* path);

// 静态库链接函数
int poly_tcc_add_lib(poly_tcc_state_t *s, const char *libpath);

#endif // POLY_TCC_H 