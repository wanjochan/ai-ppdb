/*
 * Script object system for PPDB
 */

#ifndef INFRA_SCRIPT_H
#define INFRA_SCRIPT_H

#include <stdint.h>
#include <stdbool.h>

// Basic value types
typedef int64_t I64;
typedef double F64;

typedef struct Str {
    char* data;
    size_t len;
} Str;

// Error codes
typedef enum ErrorCode {
    ERR_NONE,
    ERR_SYNTAX,    // Syntax error
    ERR_TYPE,      // Type error
    ERR_NAME,      // Name error
    ERR_MEMORY,    // Out of memory
    ERR_RUNTIME    // Runtime error
} ErrorCode;

// Object types
typedef enum Type {
    TYPE_NIL,
    TYPE_I64,
    TYPE_F64,
    TYPE_STR,
    TYPE_FUNCTION,
    TYPE_CALL,     // Function call
    TYPE_DICT,
    TYPE_ARRAY,
    TYPE_ERROR
} Type;

// Forward declarations
typedef struct Object Object;
typedef struct Dict Dict;
typedef struct Array Array;
typedef struct Function Function;
typedef struct Call Call;
typedef struct Error Error;

// Object structures
struct Dict {
    Object** keys;
    Object** values;
    size_t size;
    size_t capacity;
};

struct Array {
    Object** items;
    size_t size;
    size_t capacity;
};

struct Function {
    Object* params;    // Array of parameter names
    Object* body;      // AST of function body
    Object* env;       // Captured environment
};

struct Call {
    Object* fn;       // Function to call
    Object* args;     // Array of arguments
};

struct Error {
    ErrorCode code;   // Error type
    Str message;      // Error message
    Object* cause;    // Cause of error
    const char* file; // Source file
    int line;         // Source line
};

struct Object {
    Type type;
    union {
        I64 i64;
        F64 f64;
        Str str;
        Function fn;
        Call call;
        Dict dict;
        Array array;
        Error error;
    } value;
    size_t refs;  // Reference count
};

// Core API
Object* infra_script_eval(const char* code, Object* env);
Object* infra_script_call(Object* fn, Object* args);
Object* infra_script_get(Object* obj, const char* key);
void infra_script_set(Object* obj, const char* key, Object* val);

// Object creation
Object* infra_script_new_nil(void);
Object* infra_script_new_i64(I64 value);
Object* infra_script_new_f64(F64 value);
Object* infra_script_new_str(const char* str);
Object* infra_script_new_str_with_len(const char* str, size_t len);
Object* infra_script_new_function(Object* params, Object* body, Object* env);
Object* infra_script_new_call(Object* fn, Object* args);
Object* infra_script_new_dict(void);
Object* infra_script_new_array(void);
Object* infra_script_new_error(ErrorCode code, const char* msg, const char* file, int line);

// Memory management
void infra_script_retain(Object* obj);
void infra_script_release(Object* obj);

// Error handling
bool infra_script_is_error(Object* obj);
ErrorCode infra_script_error_code(Object* obj);
const char* infra_script_error_message(Object* obj);
const char* infra_script_error_file(Object* obj);
int infra_script_error_line(Object* obj);

// Array operations
void infra_script_array_push(Object* array, Object* item);
Object* infra_script_array_get(Object* array, size_t index);
size_t infra_script_array_size(Object* array);

// Environment operations
Object* infra_script_new_env(Object* parent);
Object* infra_script_env_get(Object* env, const char* name);
void infra_script_env_set(Object* env, const char* name, Object* value);

#endif // INFRA_SCRIPT_H 