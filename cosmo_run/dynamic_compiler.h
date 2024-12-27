#ifndef DYNAMIC_COMPILER_H
#define DYNAMIC_COMPILER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct DynamicCompiler;
struct DCompilerOptions;

/* Opaque pointer types */
typedef struct DynamicCompiler DynamicCompiler;
typedef struct DCompilerOptions DCompilerOptions;

/* Function declarations */
DynamicCompiler* dc_create(void);
void dc_destroy(DynamicCompiler* dc);
int dc_set_options(DynamicCompiler* dc, const DCompilerOptions* options);
int dc_compile(DynamicCompiler* dc, const char* code);
int dc_execute(DynamicCompiler* dc);
const char* dc_get_error(DynamicCompiler* dc);
int dc_add_symbol(DynamicCompiler* dc, const char* name, void* ptr);
void* dc_get_symbol(DynamicCompiler* dc, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* DYNAMIC_COMPILER_H */
