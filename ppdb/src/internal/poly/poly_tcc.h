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

// Memory protection flags
#define POLY_TCC_PROT_NONE  0
#define POLY_TCC_PROT_READ  1
#define POLY_TCC_PROT_WRITE 2
#define POLY_TCC_PROT_EXEC  4

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// ELF format types
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;

// ELF constants
#define EI_NIDENT 16

// ELF section types
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

// ELF symbol binding types
#define STB_LOCAL   0
#define STB_GLOBAL  1
#define STB_WEAK    2

// ELF symbol types
#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4

// Token types
typedef enum {
    TOK_EOF = -1,
    TOK_INT = 128,
    TOK_CHAR,
    TOK_VOID,
    TOK_RETURN,
    TOK_IDENT,
    TOK_STR,
    TOK_NUM,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MUL,
    TOK_DIV,
    TOK_ASSIGN,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_SEMICOLON,
    TOK_COMMA,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_BREAK,
    TOK_FOR,
    TOK_EXTERN,
    TOK_STATIC,
    TOK_UNSIGNED,
    TOK_GOTO,
    TOK_CONTINUE,
    TOK_SWITCH,
    TOK_CASE
} TokenType;

// Value types
typedef enum {
    VT_VOID = 0,
    VT_INT,
    VT_CHAR,
    VT_SHORT,
    VT_LONG,
    VT_FLOAT,
    VT_DOUBLE,
    VT_SIGNED,
    VT_UNSIGNED,
    VT_ARRAY,
    VT_PTR,
    VT_FUNC,
    VT_STRUCT,
    VT_ENUM,
    VT_STATIC,
    VT_EXTERN
} ValueType;

// Forward declarations
struct Sym;

// Token symbol structure
typedef struct TokenSym {
    struct TokenSym *next;   // next token in hash list
    struct Sym *sym;         // associated symbol
    int tok;                // token number
    int len;               // token length
    char str[1];           // token string (variable length)
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
    int v;              // symbol value or token
    struct Type {
        int t;          // type
        struct Sym *ref;// pointer to other symbol
    } type;
    union {
        int i;          // integer value
        void *ptr;      // pointer value
    } c;               // constant value
    struct Sym *next;   // next related symbol
} Sym;

// ELF header
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

// Section header
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

// Symbol table entry
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
    // Source code
    const char *source;     // source code
    int source_pos;        // current position in source code
    size_t source_len;      // source code length
    int line_num;          // current line number
    int tok;              // current token
    int tok_val;          // token value

    // Code generation
    void *code;           // code segment
    size_t code_size;     // current code size
    size_t code_capacity; // code segment capacity
    void *data;           // data segment
    size_t data_size;     // current data size
    size_t data_capacity; // data segment capacity

    // Symbol tables
    Sym *global_stack;    // global symbols
    Sym *local_stack;     // local symbols
    Sym *define_stack;    // macro definitions
    Sym *global_label_stack; // global labels
    Sym *local_label_stack;  // local labels

    // Error handling
    char error_msg[256];  // error message buffer
} poly_tcc_state_t;

//-----------------------------------------------------------------------------
// Function declarations
//-----------------------------------------------------------------------------

// State management
poly_tcc_state_t* poly_tcc_new(void);
void poly_tcc_delete(poly_tcc_state_t* s);

// Symbol management
int poly_tcc_add_symbol(poly_tcc_state_t* s, const char* name, void* addr);
void* poly_tcc_get_symbol(poly_tcc_state_t* s, const char* name);

// Code generation
int poly_tcc_compile_string(poly_tcc_state_t* s, const char* str);
int poly_tcc_run(poly_tcc_state_t* s, int argc, char** argv);

// Memory management
void* poly_tcc_malloc(size_t size);
void poly_tcc_free(void* ptr);
void* poly_tcc_mmap(void* addr, size_t size, int prot);
infra_error_t poly_tcc_munmap(void* ptr, size_t size);
infra_error_t poly_tcc_mprotect(void* ptr, size_t size, int prot);

// Path management
int poly_tcc_add_include_path(poly_tcc_state_t* s, const char* path);
int poly_tcc_add_library_path(poly_tcc_state_t* s, const char* path);

// Library management
int poly_tcc_add_lib(poly_tcc_state_t* s, const char* libpath);

// ELF parsing
int poly_tcc_parse_elf(poly_tcc_state_t* s, const char* elf_path);

// Error handling
const char* poly_tcc_get_error_msg(poly_tcc_state_t* s);

// Symbol table management (internal)
Sym* sym_push2(Sym** ps, int v, int t, int c);
Sym* sym_find2(Sym* s, int v);
void sym_free(Sym* s);
void sym_pop(Sym** ps, Sym* b);

#endif // POLY_TCC_H 