#ifndef POLY_TCC_H
#define POLY_TCC_H

//a tiny clone of tinycc, to make our ppdb to run c codes in JIT mode

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_error.h"
#include "cosmopolitan.h"  // TODO cosmo/infra later: 需要文件操作函数

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

// Value types
#define VT_INT       0x0001  // integer type
#define VT_CHAR      0x0002  // character type
#define VT_SHORT     0x0004  // short type
#define VT_VOID      0x0008  // void type
#define VT_LONG      0x0010  // long type
#define VT_FLOAT     0x0020  // float type
#define VT_DOUBLE    0x0040  // double type
#define VT_SIGNED    0x0080  // signed type
#define VT_UNSIGNED  0x0100  // unsigned type
#define VT_ARRAY     0x0200  // array type
#define VT_POINTER   0x0400  // pointer type
#define VT_FUNC      0x0800  // function type
#define VT_STRUCT    0x1000  // struct type
#define VT_ENUM      0x2000  // enum type
#define VT_STATIC    0x4000  // static type
#define VT_EXTERN    0x8000  // extern type

// Token types
#define TOK_EOF       (-1)  // end of file
#define TOK_IDENT     256   // identifier
#define TOK_INT       257   // integer constant
#define TOK_FLOAT     258   // float constant
#define TOK_STR       259   // string constant
#define TOK_CHAR      260   // character constant
#define TOK_IF        261   // if
#define TOK_ELSE      262   // else
#define TOK_WHILE     263   // while
#define TOK_BREAK     264   // break
#define TOK_RETURN    265   // return
#define TOK_FOR       266   // for
#define TOK_EXTERN    267   // extern
#define TOK_STATIC    268   // static
#define TOK_UNSIGNED  269   // unsigned
#define TOK_GOTO      270   // goto
#define TOK_CONTINUE  271   // continue
#define TOK_SWITCH    272   // switch
#define TOK_CASE      273   // case

// Forward declarations
struct Sym;

// Token symbol
typedef struct TokenSym {
    struct TokenSym *next;   // next token in hash bucket
    struct Sym *sym_label;   // associated label
    int tok;                 // token number
    int len;                 // token length
    char str[1];            // token string
} TokenSym;

// Type structure
typedef struct CType {
    int t;            // type
    struct Sym *ref;  // reference to a symbol
} CType;

// Value structure
typedef union CValue {
    int i;           // integer value
    float f;         // float value
    double d;        // double value
    char *str;       // string value
    void *ptr;       // pointer value
} CValue;

// Symbol structure
typedef struct Sym {
    int v;           // symbol value
    CType type;      // symbol type
    CValue c;        // symbol constant value
    struct Sym *next;  // next symbol in stack
} Sym;

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

// TCC state structure
typedef struct poly_tcc_state {
    // Memory segments
    void *code;              // code segment
    size_t code_size;        // code size
    size_t code_capacity;    // code capacity
    void *data;              // data segment
    size_t data_size;        // data size
    size_t data_capacity;    // data capacity

    // Symbol tables
    Sym *global_stack;       // global symbol stack
    Sym *local_stack;        // local symbol stack
    Sym *define_stack;       // macro definition stack
    Sym *global_label_stack; // global label stack
    Sym *local_label_stack;  // local label stack

    // Source code
    const char *source;      // current source code
    int source_len;          // source code length
    int source_pos;          // current position in source
    int line_num;           // current line number
    int tok;                // current token
    CValue tokc;           // token value

    // Error message
    char error_msg[256];
} poly_tcc_state_t;

//-----------------------------------------------------------------------------
// Function declarations
//-----------------------------------------------------------------------------

// State management
poly_tcc_state_t* poly_tcc_new(void);
void poly_tcc_delete(poly_tcc_state_t* s);

// Symbol management
int poly_tcc_add_symbol(poly_tcc_state_t *s, const char *name, void *addr);
void* poly_tcc_get_symbol(poly_tcc_state_t *s, const char *name);

// Code generation
int poly_tcc_compile_string(poly_tcc_state_t *s, const char *str);
int poly_tcc_run(poly_tcc_state_t *s, int argc, char **argv);

// Memory management
void* poly_tcc_malloc(size_t size);
void poly_tcc_free(void *ptr);
void* poly_tcc_mmap(void *addr, size_t size, int prot);
infra_error_t poly_tcc_munmap(void *ptr, size_t size);
infra_error_t poly_tcc_mprotect(void *ptr, size_t size, int prot);

// Path management
int poly_tcc_add_include_path(poly_tcc_state_t* s, const char* path);
int poly_tcc_add_library_path(poly_tcc_state_t* s, const char* path);

// Library management
int poly_tcc_add_lib(poly_tcc_state_t *s, const char *libpath);

// ELF parsing
int poly_tcc_parse_elf(poly_tcc_state_t* s, const char* elf_path);

// Error handling
const char* poly_tcc_get_error_msg(poly_tcc_state_t *s);

// Memory protection flags
#define POLY_TCC_PROT_NONE  0
#define POLY_TCC_PROT_READ  1
#define POLY_TCC_PROT_WRITE 2
#define POLY_TCC_PROT_EXEC  4

// Symbol table management (internal)
Sym* sym_push2(Sym **ps, int v, int t, int c);
Sym* sym_find2(Sym *s, int v);
void sym_free(Sym *s);
void sym_pop(Sym **ps, Sym *b);

#endif // POLY_TCC_H 