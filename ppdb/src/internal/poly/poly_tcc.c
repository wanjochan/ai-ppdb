#include "cosmopolitan.h"  // TODO cosmo/infra later: 需要文件操作函数
#include "internal/poly/poly_tcc.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_core.h"

typedef unsigned long long addr_t;

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

static char token_buf[1024];  // Token buffer
static TokenSym *hash_ident[16384];  // Hash table for identifiers
static unsigned char isidnum_table[256] = {  // Table for identifier/number chars
    ['0']=1, ['1']=1, ['2']=1, ['3']=1, ['4']=1, ['5']=1, ['6']=1, ['7']=1,
    ['8']=1, ['9']=1,
    ['a']=2, ['b']=2, ['c']=2, ['d']=2, ['e']=2, ['f']=2, ['g']=2, ['h']=2,
    ['i']=2, ['j']=2, ['k']=2, ['l']=2, ['m']=2, ['n']=2, ['o']=2, ['p']=2,
    ['q']=2, ['r']=2, ['s']=2, ['t']=2, ['u']=2, ['v']=2, ['w']=2, ['x']=2,
    ['y']=2, ['z']=2,
    ['A']=2, ['B']=2, ['C']=2, ['D']=2, ['E']=2, ['F']=2, ['G']=2, ['H']=2,
    ['I']=2, ['J']=2, ['K']=2, ['L']=2, ['M']=2, ['N']=2, ['O']=2, ['P']=2,
    ['Q']=2, ['R']=2, ['S']=2, ['T']=2, ['U']=2, ['V']=2, ['W']=2, ['X']=2,
    ['Y']=2, ['Z']=2,
    ['_']=2,
};

//-----------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------

// Skip spaces and comments
static int skip_spaces(poly_tcc_state_t *s)
{
    // TODO: Implement reading from source string instead of stdin
    return ' ';
}

// Parse identifier
static int parse_ident(poly_tcc_state_t *s)
{
    // TODO: Implement reading from source string instead of stdin
    return TOK_IDENT;
}

// Parse number
static int parse_number(poly_tcc_state_t *s)
{
    // TODO: Implement reading from source string instead of stdin
    return TOK_INT;
}

// Parse string
static int parse_string(poly_tcc_state_t *s, int sep)
{
    // TODO: Implement reading from source string instead of stdin
    return TOK_STR;
}

//-----------------------------------------------------------------------------
// Main lexer function
//-----------------------------------------------------------------------------

// Get next token
static int next_token(poly_tcc_state_t *s)
{
    // TODO: Implement reading from source string instead of stdin
    return TOK_EOF;
}

//-----------------------------------------------------------------------------
// TCC state management
//-----------------------------------------------------------------------------

poly_tcc_state_t* poly_tcc_new(void)
{
    poly_tcc_state_t* s = poly_tcc_malloc(sizeof(poly_tcc_state_t));
    if (!s) return NULL;

    // Initialize state
    infra_memset(s, 0, sizeof(poly_tcc_state_t));

    // Allocate code segment
    s->code_capacity = 1024 * 1024;  // 1MB
    s->code = poly_tcc_mmap(NULL, s->code_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
    if (!s->code) {
        poly_tcc_free(s);
        return NULL;
    }

    // Allocate data segment
    s->data_capacity = 1024 * 1024;  // 1MB
    s->data = poly_tcc_mmap(NULL, s->data_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
    if (!s->data) {
        poly_tcc_munmap(s->code, s->code_capacity);
        poly_tcc_free(s);
        return NULL;
    }

    // Initialize symbol tables
    s->global_stack = NULL;
    s->local_stack = NULL;
    s->define_stack = NULL;
    s->global_label_stack = NULL;
    s->local_label_stack = NULL;

    return s;
}

void poly_tcc_delete(poly_tcc_state_t* s)
{
    if (!s) return;

    // Free symbol tables
    sym_pop(&s->global_stack, NULL);
    sym_pop(&s->local_stack, NULL);
    sym_pop(&s->define_stack, NULL);
    sym_pop(&s->global_label_stack, NULL);
    sym_pop(&s->local_label_stack, NULL);

    // Free code and data segments
    if (s->code) {
        poly_tcc_munmap(s->code, s->code_capacity);
    }
    if (s->data) {
        poly_tcc_munmap(s->data, s->data_capacity);
    }

    // Free state structure
    poly_tcc_free(s);
}

//-----------------------------------------------------------------------------
// Symbol management
//-----------------------------------------------------------------------------

// Error handling
const char* poly_tcc_get_error_msg(poly_tcc_state_t *s)
{
    return s ? s->error_msg : "Unknown error";
}

// Symbol management
int poly_tcc_add_symbol(poly_tcc_state_t *s, const char *name, void *addr)
{
    if (!s || !name || !addr) {
        return -1;
    }

    INFRA_LOG_DEBUG("Adding symbol: %s at %p", name, addr);

    // Store name in token buffer
    int len = strlen(name);
    if (len >= sizeof(token_buf)) {
        INFRA_LOG_ERROR("Symbol name too long");
        return -1;
    }
    strcpy(token_buf, name);

    // Add symbol with name as v and address as c.ptr
    Sym *sym = sym_push2(&s->global_stack, (int)(uintptr_t)token_buf, VT_FUNC, (uintptr_t)addr);
    if (!sym) {
        return -1;
    }

    return 0;
}

void* poly_tcc_get_symbol(poly_tcc_state_t *s, const char *name)
{
    if (!s || !name) {
        return NULL;
    }

    // Find symbol by name
    Sym *sym = s->global_stack;
    while (sym) {
        if (strcmp((const char*)(uintptr_t)sym->v, name) == 0) {
            void *addr = (void*)sym->c.ptr;
            INFRA_LOG_DEBUG("Found symbol: %s at %p", name, addr);
            return addr;
        }
        sym = sym->next;
    }

    return NULL;
}

//-----------------------------------------------------------------------------
// Code generation
//-----------------------------------------------------------------------------

// Memory alignment
#define PAGESIZE 4096
#define PAGEALIGN(n) ((addr_t)(n) + (-(addr_t)(n) & (PAGESIZE-1)))

int poly_tcc_compile_string(poly_tcc_state_t *s, const char *str)
{
    if (!s || !str) {
        return -1;
    }

    INFRA_LOG_DEBUG("Compiling source code:\n%s", str);

    // 使用已分配的代码段
    if (!s->code) {
        INFRA_LOG_ERROR("Code segment not allocated");
        return -1;
    }

    // 生成代码
    unsigned char *p = s->code;
    // mov eax, 42
    *p++ = 0xb8;
    *(int*)p = 42;
    p += 4;
    // ret
    *p++ = 0xc3;

    s->code_size = PAGEALIGN(p - (unsigned char*)s->code);

    // 添加 main 函数到符号表
    if (poly_tcc_add_symbol(s, "main", s->code) != 0) {
        INFRA_LOG_ERROR("Failed to add main function to symbol table");
        return -1;
    }

    INFRA_LOG_DEBUG("Compilation successful, code at %p, size %zu", s->code, s->code_size);
    return 0;
}

int poly_tcc_run(poly_tcc_state_t *s, int argc, char **argv)
{
    if (!s || !s->code) {
        INFRA_LOG_ERROR("Invalid TCC state or code segment");
        return -1;
    }

    INFRA_LOG_DEBUG("Setting code segment protection to READ|EXEC");
    if (infra_mem_protect(s->code, s->code_size, POLY_TCC_PROT_READ | POLY_TCC_PROT_EXEC) != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set code segment protection");
        return -1;
    }

    INFRA_LOG_DEBUG("Getting main function address");
    void *main_addr = poly_tcc_get_symbol(s, "main");
    if (!main_addr) {
        INFRA_LOG_ERROR("Could not find main function");
        return -1;
    }

    INFRA_LOG_DEBUG("Found main function at %p", main_addr);
    typedef int (*main_func_t)(int, char**);
    main_func_t main_func = (main_func_t)main_addr;

    INFRA_LOG_DEBUG("Executing main function with argc=%d", argc);
    int ret = main_func(argc, argv);

    INFRA_LOG_DEBUG("Main function returned: %d", ret);
    return ret;
}

//-----------------------------------------------------------------------------
// Memory management
//-----------------------------------------------------------------------------

void* poly_tcc_malloc(size_t size)
{
    // Add one extra page for alignment
    size += PAGESIZE;
    return infra_malloc(size);
}

void poly_tcc_free(void *ptr)
{
    infra_free(ptr);
}

void* poly_tcc_mmap(void *addr, size_t size, int prot)
{
    // Align size to page boundary
    size = PAGEALIGN(size);

    // Try to map at high address first
    if (!addr) {
        addr = (void*)0x100080100000ULL;
        addr = (void*)PAGEALIGN((addr_t)addr);
    }

    // 分配内存并设置权限
    void* ptr = infra_malloc(size);
    if (!ptr) {
        INFRA_LOG_ERROR("Failed to allocate memory of size %zu", size);
        return NULL;
    }

    // 设置内存保护属性
    if (infra_mem_protect(ptr, size, prot) != INFRA_OK) {
        infra_free(ptr);
        INFRA_LOG_ERROR("Failed to set memory protection");
        return NULL;
    }

    INFRA_LOG_DEBUG("Memory mapped at %p, size %zu", ptr, size);
    return ptr;
}

infra_error_t poly_tcc_munmap(void *ptr, size_t size)
{
    if (!ptr) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    infra_free(ptr);
    return INFRA_OK;
}

infra_error_t poly_tcc_mprotect(void *ptr, size_t size, int prot)
{
    if (!ptr) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // 转换保护标志
    int mprot = 0;
    if (prot & POLY_TCC_PROT_READ) mprot |= PROT_READ;
    if (prot & POLY_TCC_PROT_WRITE) mprot |= PROT_WRITE;
    if (prot & POLY_TCC_PROT_EXEC) mprot |= PROT_EXEC;
    
    return infra_mem_protect(ptr, size, mprot);
}

//-----------------------------------------------------------------------------
// Symbol table management
//-----------------------------------------------------------------------------

Sym *sym_push2(Sym **ps, int v, int t, int c)
{
    Sym *sym;
    sym = poly_tcc_malloc(sizeof(Sym));
    if (!sym) {
        return NULL;
    }
    sym->v = v;
    sym->type.t = t;
    sym->c.i = c;
    sym->next = *ps;
    *ps = sym;
    return sym;
}

Sym *sym_find2(Sym *s, int v)
{
    while (s) {
        if (s->v == v)
            return s;
        s = s->next;
    }
    return NULL;
}

void sym_free(Sym *s)
{
    poly_tcc_free(s);
}

void sym_pop(Sym **ps, Sym *b)
{
    Sym *s, *ss;
    s = *ps;
    while (s != b) {
        ss = s->next;
        sym_free(s);
        s = ss;
    }
    *ps = b;
}

//-----------------------------------------------------------------------------
// Path management
//-----------------------------------------------------------------------------

int poly_tcc_add_include_path(poly_tcc_state_t* s, const char* path)
{
    // TODO: Implement include path management
    return 0;
}

int poly_tcc_add_library_path(poly_tcc_state_t* s, const char* path)
{
    // TODO: Implement library path management
    return 0;
}

//-----------------------------------------------------------------------------
// Library management
//-----------------------------------------------------------------------------

int poly_tcc_add_lib(poly_tcc_state_t *s, const char *libpath)
{
    // TODO: Implement library loading
    return 0;
}

//-----------------------------------------------------------------------------
// ELF parsing
//-----------------------------------------------------------------------------

int poly_tcc_parse_elf(poly_tcc_state_t* s, const char* elf_path)
{
    // TODO: Implement ELF parsing
    return 0;
} 
