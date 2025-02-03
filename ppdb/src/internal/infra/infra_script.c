/*
 * Script object system implementation
 */

#include "infra_script.h"
#include <stdlib.h>
#include <string.h>

// Object creation
Object infra_script_new_i64(I64 value) {
    Object obj = malloc(sizeof(struct Object));
    obj->type = TYPE_I64;
    obj->value.i64 = value;
    return obj;
}

Object infra_script_new_u64(U64 value) {
    Object obj = malloc(sizeof(struct Object));
    obj->type = TYPE_U64;
    obj->value.u64 = value;
    return obj;
}

Object infra_script_new_f64(F64 value) {
    Object obj = malloc(sizeof(struct Object));
    obj->type = TYPE_F64;
    obj->value.f64 = value;
    return obj;
}

Object infra_script_new_str(const char* s) {
    Object obj = malloc(sizeof(struct Object));
    obj->type = TYPE_STR;
    size_t len = strlen(s);
    obj->value.str.ptr = malloc(len + 1);
    memcpy(obj->value.str.ptr, s, len + 1);
    obj->value.str.len = len;
    return obj;
}

Function infra_script_new_func(const char* name, void* func) {
    Function f = malloc(sizeof(struct Function));
    f->type = TYPE_FUNC;
    size_t len = strlen(name);
    f->name = malloc(len + 1);
    memcpy(f->name, name, len + 1);
    f->func = func;
    return f;
}

Module infra_script_new_module(const char* name) {
    Module mod = malloc(sizeof(struct Module));
    mod->type = TYPE_MODULE;
    size_t len = strlen(name);
    mod->name = malloc(len + 1);
    memcpy(mod->name, name, len + 1);
    mod->functions = NULL;
    mod->count = 0;
    return mod;
}

// Function management
void infra_script_add_function(Module mod, const char* name, Function func) {
    mod->functions = realloc(mod->functions, 
        (mod->count + 1) * sizeof(*mod->functions));
    
    size_t len = strlen(name);
    mod->functions[mod->count].name = malloc(len + 1);
    memcpy(mod->functions[mod->count].name, name, len + 1);
    mod->functions[mod->count].func = func;
    mod->count++;
}

// Function call
Object infra_script_call(Function func, Object arg) {
    // Type check and conversion should be added here
    typedef Object (*ScriptFunc)(Object);
    return ((ScriptFunc)func->func)(arg);
}

// Cleanup
void infra_script_destroy(Object obj) {
    if (!obj) return;
    
    switch (obj->type) {
        case TYPE_STR:
            free(obj->value.str.ptr);
            break;
        case TYPE_FUNC:
            free(((Function)obj)->name);
            break;
        case TYPE_MODULE:
            {
                Module mod = (Module)obj;
                for (int i = 0; i < mod->count; i++) {
                    free(mod->functions[i].name);
                }
                free(mod->functions);
                free(mod->name);
            }
            break;
        default:
            break;
    }
    free(obj);
} 