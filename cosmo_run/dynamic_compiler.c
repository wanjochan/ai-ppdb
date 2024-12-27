#include "dynamic_compiler.h"
#include <stdlib.h>
#include <string.h>

/* Default memory size (4MB) */
#define DEFAULT_MEMORY_SIZE (4 * 1024 * 1024)

/* Internal structures */
struct Symbol {
    char* name;
    void* ptr;
    struct Symbol* next;
};

struct DCompilerOptions {
    int optimization_level;
    bool debug_info;
    const char* include_path;
};

struct DynamicCompiler {
    void* memory;
    size_t memory_size;
    char* error_msg;
    struct DCompilerOptions options;
    struct Symbol* symbols;
};

DynamicCompiler* dc_create(void) {
    struct DynamicCompiler* dc = (struct DynamicCompiler*)malloc(sizeof(struct DynamicCompiler));
    if (!dc) return NULL;

    /* Initialize memory */
    dc->memory_size = DEFAULT_MEMORY_SIZE;
    dc->memory = malloc(dc->memory_size);
    if (!dc->memory) {
        free(dc);
        return NULL;
    }

    /* Initialize other fields */
    dc->error_msg = NULL;
    dc->symbols = NULL;
    dc->options.optimization_level = 0;
    dc->options.debug_info = false;
    dc->options.include_path = NULL;

    return dc;
}

void dc_destroy(DynamicCompiler* dc) {
    if (dc) {
        if (dc->memory) free(dc->memory);
        if (dc->error_msg) free(dc->error_msg);
        
        /* Clean up symbol table */
        struct Symbol* sym = dc->symbols;
        while (sym) {
            struct Symbol* next = sym->next;
            free(sym->name);
            free(sym);
            sym = next;
        }
        
        free(dc);
    }
}

int dc_set_options(DynamicCompiler* dc, const struct DCompilerOptions* options) {
    if (!dc || !options) return -1;
    
    /* Copy options */
    dc->options = *options;
    return 0;
}

int dc_add_symbol(DynamicCompiler* dc, const char* name, void* ptr) {
    if (!dc || !name || !ptr) return -1;

    /* Create new symbol */
    struct Symbol* sym = (struct Symbol*)malloc(sizeof(struct Symbol));
    if (!sym) return -1;

    sym->name = _strdup(name);
    if (!sym->name) {
        free(sym);
        return -1;
    }

    sym->ptr = ptr;
    sym->next = dc->symbols;
    dc->symbols = sym;

    return 0;
}

void* dc_get_symbol(DynamicCompiler* dc, const char* name) {
    if (!dc || !name) return NULL;

    struct Symbol* sym = dc->symbols;
    while (sym) {
        if (strcmp(sym->name, name) == 0) {
            return sym->ptr;
        }
        sym = sym->next;
    }

    return NULL;
}

int dc_compile(DynamicCompiler* dc, const char* code) {
    if (!dc || !code) return -1;

    /* TODO: Implement actual compilation logic
     * 1. Lexical analysis
     * 2. Syntax analysis
     * 3. Code generation
     * 4. Linking
     */

    /* Temporary implementation: copy code to memory */
    size_t code_len = strlen(code);
    if (code_len > dc->memory_size) {
        dc->error_msg = _strdup("Code too long");
        return -1;
    }

    memcpy(dc->memory, code, code_len);
    return 0;
}

int dc_execute(DynamicCompiler* dc) {
    if (!dc || !dc->memory) return -1;

    /* TODO: Implement actual execution logic
     * 1. Set memory page as executable
     * 2. Call entry point function
     */
    
    typedef int (*MainFunc)(void);
    MainFunc main_func = (MainFunc)dc->memory;
    
    /* Note: Need to ensure memory is executable */
    return main_func();
}

const char* dc_get_error(DynamicCompiler* dc) {
    return dc ? dc->error_msg : "Invalid compiler instance";
}
