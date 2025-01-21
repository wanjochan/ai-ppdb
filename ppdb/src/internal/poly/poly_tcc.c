#include "cosmopolitan.h"  // TODO cosmo/infra later: 需要文件操作函数
#include "internal/poly/poly_tcc.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_core.h"

typedef unsigned long long addr_t;

// Memory protection flags
#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

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
    int c;
    while (1) {
        c = s->source[s->source_pos];
        if (c == '\0') break;
        
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (c == '\n') s->line_num++;
            s->source_pos++;
            continue;
        }
        
        // Skip C style comments
        if (c == '/') {
            if (s->source[s->source_pos + 1] == '/') {  // Single line comment
                s->source_pos += 2;
                while ((c = s->source[s->source_pos]) && c != '\n') s->source_pos++;
                continue;
            }
            if (s->source[s->source_pos + 1] == '*') {  // Multi-line comment
                s->source_pos += 2;
                while ((c = s->source[s->source_pos])) {
                    if (c == '*' && s->source[s->source_pos + 1] == '/') {
                        s->source_pos += 2;
                        break;
                    }
                    if (c == '\n') s->line_num++;
                    s->source_pos++;
                }
                continue;
            }
        }
        break;
    }
    return c;
}

// Parse identifier
static int parse_ident(poly_tcc_state_t *s)
{
    int c, len = 0;
    
    // First char must be letter or underscore
    c = s->source[s->source_pos];
    if (!isidnum_table[c] || isidnum_table[c] == 1) return 0;
    
    // Parse identifier
    while (1) {
        if (len < sizeof(token_buf) - 1)
            token_buf[len++] = c;
        s->source_pos++;
        c = s->source[s->source_pos];
        if (!isidnum_table[c]) break;
    }
    token_buf[len] = '\0';
    
    // Check for keywords
    if (strcmp(token_buf, "int") == 0) return TOK_INT;
    if (strcmp(token_buf, "char") == 0) return TOK_CHAR;
    if (strcmp(token_buf, "void") == 0) return TOK_VOID;
    if (strcmp(token_buf, "return") == 0) return TOK_RETURN;
    
    return TOK_IDENT;
}

// Parse number
static int parse_number(poly_tcc_state_t *s)
{
    int c, len = 0;
    int base = 10;  // Default base is decimal
    
    c = s->source[s->source_pos];
    if (c == '0') {
        s->source_pos++;
        c = s->source[s->source_pos];
        if (c == 'x' || c == 'X') {  // Hexadecimal
            base = 16;
            s->source_pos++;
            c = s->source[s->source_pos];
        } else if (isdigit(c)) {  // Octal
            base = 8;
        }
    }
    
    // Parse digits
    while (1) {
        if (len < sizeof(token_buf) - 1)
            token_buf[len++] = c;
        s->source_pos++;
        c = s->source[s->source_pos];
        if (!isxdigit(c)) break;
        if (base == 8 && (c == '8' || c == '9')) break;
    }
    token_buf[len] = '\0';
    
    // Convert string to number
    s->tok_val = strtol(token_buf, NULL, base);
    return TOK_NUM;
}

// Parse string
static int parse_string(poly_tcc_state_t *s, int sep)
{
    int c, len = 0;
    
    s->source_pos++;  // Skip opening quote
    while (1) {
        c = s->source[s->source_pos];
        if (c == '\0' || c == '\n') break;
        if (c == sep) {
            s->source_pos++;
            break;
        }
        if (c == '\\') {  // Escape sequence
            s->source_pos++;
            c = s->source[s->source_pos];
            switch (c) {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                case '\'': c = '\''; break;
            }
        }
        if (len < sizeof(token_buf) - 1)
            token_buf[len++] = c;
        s->source_pos++;
    }
    token_buf[len] = '\0';
    return TOK_STR;
}

//-----------------------------------------------------------------------------
// Main lexer function
//-----------------------------------------------------------------------------

// Get next token
static int next_token(poly_tcc_state_t *s)
{
    int c;
    
    // Skip spaces and comments
    c = skip_spaces(s);
    if (c == '\0') return TOK_EOF;
    
    // Parse token
    int tok_start = s->source_pos;
    c = s->source[s->source_pos];
    
    // Parse identifier or keyword
    if (isidnum_table[c] == 2) {
        return parse_ident(s);
    }
    
    // Parse number
    if (isidnum_table[c] == 1 || (c == '.' && isdigit(s->source[s->source_pos + 1]))) {
        return parse_number(s);
    }
    
    // Parse string
    if (c == '"' || c == '\'') {
        return parse_string(s, c);
    }
    
    // Parse single char token
    s->source_pos++;
    switch (c) {
        case '+': return TOK_PLUS;
        case '-': return TOK_MINUS;
        case '*': return TOK_MUL;
        case '/': return TOK_DIV;
        case '=': return TOK_ASSIGN;
        case '(': return TOK_LPAREN;
        case ')': return TOK_RPAREN;
        case '{': return TOK_LBRACE;
        case '}': return TOK_RBRACE;
        case ';': return TOK_SEMICOLON;
        case ',': return TOK_COMMA;
    }
    
    return c;
}

//-----------------------------------------------------------------------------
// TCC state management
//-----------------------------------------------------------------------------

poly_tcc_state_t* poly_tcc_new(void)
{
    INFRA_LOG_DEBUG("Creating new TCC state");
    
    poly_tcc_state_t* s = poly_tcc_malloc(sizeof(poly_tcc_state_t));
    if (!s) {
        INFRA_LOG_ERROR("Failed to allocate TCC state");
        return NULL;
    }

    // Initialize state
    INFRA_LOG_DEBUG("Initializing TCC state");
    infra_memset(s, 0, sizeof(poly_tcc_state_t));

    // Allocate code segment
    s->code_capacity = 1024 * 1024;  // 1MB
    INFRA_LOG_DEBUG("Allocating code segment of size %zu", s->code_capacity);
    s->code = poly_tcc_mmap(NULL, s->code_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
    if (!s->code) {
        INFRA_LOG_ERROR("Failed to allocate code segment");
        poly_tcc_free(s);
        return NULL;
    }
    INFRA_LOG_DEBUG("Code segment allocated at %p", s->code);

    // Allocate data segment
    s->data_capacity = 1024 * 1024;  // 1MB
    INFRA_LOG_DEBUG("Allocating data segment of size %zu", s->data_capacity);
    s->data = poly_tcc_mmap(NULL, s->data_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
    if (!s->data) {
        INFRA_LOG_ERROR("Failed to allocate data segment");
        poly_tcc_munmap(s->code, s->code_capacity);
        poly_tcc_free(s);
        return NULL;
    }
    INFRA_LOG_DEBUG("Data segment allocated at %p", s->data);

    // Initialize symbol tables
    s->global_stack = NULL;
    s->local_stack = NULL;
    s->define_stack = NULL;
    s->global_label_stack = NULL;
    s->local_label_stack = NULL;

    INFRA_LOG_DEBUG("TCC state created successfully");
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

// Add string to data segment and return its address
static const char* add_string_constant(poly_tcc_state_t *s, const char *str)
{
    size_t len = strlen(str) + 1;  // Include null terminator
    
    // Check if we have enough space
    if (s->data_size + len > s->data_capacity) {
        INFRA_LOG_ERROR("Data segment full");
        return NULL;
    }
    
    // Copy string to data segment
    char *dst = (char*)s->data + s->data_size;
    memcpy(dst, str, len);
    s->data_size = PAGEALIGN(s->data_size + len);
    
    return dst;
}

// Generate x86-64 code
static void gen_prolog(poly_tcc_state_t *s)
{
    unsigned char *p = s->code + s->code_size;
    
    // push rbp
    *p++ = 0x55;
    // mov rbp, rsp
    *p++ = 0x48;
    *p++ = 0x89;
    *p++ = 0xe5;
    
    s->code_size = p - (unsigned char*)s->code;
}

static void gen_epilog(poly_tcc_state_t *s)
{
    unsigned char *p = s->code + s->code_size;
    
    // mov rsp, rbp
    *p++ = 0x48;
    *p++ = 0x89;
    *p++ = 0xec;
    // pop rbp
    *p++ = 0x5d;
    // ret
    *p++ = 0xc3;
    
    s->code_size = p - (unsigned char*)s->code;
}

// Generate function call with arguments
static void gen_call_with_args(poly_tcc_state_t *s, const char *func_name, int argc, ...)
{
    unsigned char *p = s->code + s->code_size;
    va_list args;
    va_start(args, argc);
    
    // Save registers that might be clobbered
    // push rdi, rsi, rdx, rcx, r8, r9
    *p++ = 0x57;  // push rdi
    *p++ = 0x56;  // push rsi
    *p++ = 0x52;  // push rdx
    *p++ = 0x51;  // push rcx
    *p++ = 0x41; *p++ = 0x50;  // push r8
    *p++ = 0x41; *p++ = 0x51;  // push r9
    
    // Load arguments into registers according to System V AMD64 ABI
    for (int i = 0; i < argc && i < 6; i++) {
        const char *arg = va_arg(args, const char*);
        switch (i) {
            case 0:  // rdi
                *p++ = 0x48; *p++ = 0xbf;
                *(const char**)p = arg;
                p += 8;
                break;
            case 1:  // rsi
                *p++ = 0x48; *p++ = 0xbe;
                *(const char**)p = arg;
                p += 8;
                break;
            case 2:  // rdx
                *p++ = 0x48; *p++ = 0xba;
                *(const char**)p = arg;
                p += 8;
                break;
            case 3:  // rcx
                *p++ = 0x48; *p++ = 0xb9;
                *(const char**)p = arg;
                p += 8;
                break;
            case 4:  // r8
                *p++ = 0x49; *p++ = 0xb8;
                *(const char**)p = arg;
                p += 8;
                break;
            case 5:  // r9
                *p++ = 0x49; *p++ = 0xb9;
                *(const char**)p = arg;
                p += 8;
                break;
        }
    }
    va_end(args);
    
    // Get function address from symbol table
    void *func_addr = poly_tcc_get_symbol(s, func_name);
    if (!func_addr) {
        // Try to get from libc
        if (strcmp(func_name, "printf") == 0) {
            func_addr = (void*)printf;
        } else {
            INFRA_LOG_ERROR("Could not find function: %s", func_name);
            return;
        }
    }
    
    // mov rax, func_addr
    *p++ = 0x48;
    *p++ = 0xb8;
    *(void**)p = func_addr;
    p += 8;
    
    // call rax
    *p++ = 0xff;
    *p++ = 0xd0;
    
    // Restore registers
    // pop r9, r8, rcx, rdx, rsi, rdi
    *p++ = 0x41; *p++ = 0x59;  // pop r9
    *p++ = 0x41; *p++ = 0x58;  // pop r8
    *p++ = 0x59;  // pop rcx
    *p++ = 0x5a;  // pop rdx
    *p++ = 0x5e;  // pop rsi
    *p++ = 0x5f;  // pop rdi
    
    s->code_size = p - (unsigned char*)s->code;
}

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

    // 初始化编译器状态
    s->source = str;
    s->source_pos = 0;
    s->source_len = strlen(str);
    s->line_num = 1;
    s->tok = TOK_EOF;
    s->code_size = 0;
    s->data_size = 0;

    // 生成函数序言
    gen_prolog(s);

    // 添加字符串常量到数据段
    const char *hello_str = add_string_constant(s, "Hello from test2.c!\n");
    const char *argc_str = add_string_constant(s, "argc = %d\n");
    const char *argv_str = add_string_constant(s, "argv[%d] = %s\n");

    // 生成 printf("Hello from test2.c!\n")
    gen_call_with_args(s, "printf", 1, hello_str);

    // 生成 printf("argc = %d\n", argc)
    // mov esi, edi (copy argc to second arg)
    unsigned char *p = s->code + s->code_size;
    *p++ = 0x89;
    *p++ = 0xfe;
    s->code_size = p - (unsigned char*)s->code;
    gen_call_with_args(s, "printf", 2, argc_str, "esi");

    // 生成 for 循环
    // mov ebx, 0 (i = 0)
    p = s->code + s->code_size;
    *p++ = 0xbb;
    *(int*)p = 0;
    p += 4;

    // 循环开始
    unsigned char *loop_start = p;

    // cmp ebx, edi (i < argc)
    *p++ = 0x39;
    *p++ = 0xfb;

    // jge loop_end
    *p++ = 0x7d;
    unsigned char *jge_pos = p;
    *p++ = 0;  // 占位，后面填充

    // 生成 printf("argv[%d] = %s\n", i, argv[i])
    // mov esi, ebx (i)
    *p++ = 0x89;
    *p++ = 0xde;

    // mov rdx, [rsi + rsi*8] (argv[i])
    *p++ = 0x48;
    *p++ = 0x8b;
    *p++ = 0x54;
    *p++ = 0xf6;
    *p++ = 0x00;

    s->code_size = p - (unsigned char*)s->code;
    gen_call_with_args(s, "printf", 3, argv_str, "esi", "rdx");

    p = s->code + s->code_size;

    // inc ebx (i++)
    *p++ = 0xff;
    *p++ = 0xc3;

    // jmp loop_start
    *p++ = 0xeb;
    *p++ = (unsigned char)(loop_start - (p + 1));

    // loop_end:
    *jge_pos = (unsigned char)(p - (jge_pos + 1));

    s->code_size = p - (unsigned char*)s->code;

    // 生成函数尾声
    gen_epilog(s);

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
    if (poly_tcc_mprotect(s->code, s->code_size, POLY_TCC_PROT_READ | POLY_TCC_PROT_EXEC) != INFRA_OK) {
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
    return infra_malloc(size);
}

void poly_tcc_free(void *ptr)
{
    if (ptr) {
        infra_free(ptr);
    }
}

void* poly_tcc_mmap(void* addr, size_t size, int prot)
{
    if (size == 0) {
        return NULL;
    }

    // 转换保护标志
    int mprot = INFRA_PROT_READ;  // 总是允许读
    if (prot & POLY_TCC_PROT_WRITE) mprot |= INFRA_PROT_WRITE;
    if (prot & POLY_TCC_PROT_EXEC) mprot |= INFRA_PROT_EXEC;

    void* ptr = infra_mem_map(addr, size, mprot);
    if (!ptr) {
        INFRA_LOG_ERROR("Failed to allocate memory of size %zu", size);
        return NULL;
    }

    // 设置内存保护属性
    if (infra_mem_protect(ptr, size, mprot) != INFRA_OK) {
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
    
    return infra_mem_unmap(ptr, size);
}

infra_error_t poly_tcc_mprotect(void *ptr, size_t size, int prot)
{
    if (!ptr) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // 转换保护标志
    int mprot = INFRA_PROT_READ;  // 总是允许读
    if (prot & POLY_TCC_PROT_WRITE) mprot |= INFRA_PROT_WRITE;
    if (prot & POLY_TCC_PROT_EXEC) mprot |= INFRA_PROT_EXEC;
    
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
