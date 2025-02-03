/*
 * Script object system for PPDB
 */

#ifndef INFRA_SCRIPT_H
#define INFRA_SCRIPT_H

#include <stddef.h>
#include <stdint.h>

// Basic value types
typedef int64_t I64;
typedef uint64_t U64;
typedef double F64;
typedef struct {
    char* ptr;
    size_t len;
} Str;

// Object types
typedef enum {
    TYPE_NULL,
    TYPE_I64,
    TYPE_U64,
    TYPE_F64,
    TYPE_STR,
    TYPE_FUNC,
    TYPE_MODULE
} Type;

// Object structures
typedef struct Object {
    Type type;
    union {
        I64 i64;
        U64 u64;
        F64 f64;
        Str str;
        void* ptr;
    } value;
} *Object;

typedef struct Function {
    Type type;
    char* name;
    void* func;
} *Function;

typedef struct Module {
    Type type;
    char* name;
    struct {
        char* name;
        Function func;
    } *functions;
    int count;
} *Module;

// Core API
Object infra_script_new_i64(I64 value);
Object infra_script_new_u64(U64 value);
Object infra_script_new_f64(F64 value);
Object infra_script_new_str(const char* s);
Function infra_script_new_func(const char* name, void* func);
Module infra_script_new_module(const char* name);

void infra_script_add_function(Module mod, const char* name, Function func);
Object infra_script_call(Function func, Object arg);
void infra_script_destroy(Object obj);

#endif // INFRA_SCRIPT_H 