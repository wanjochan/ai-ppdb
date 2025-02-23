/**
 * PolyxScript - A lightweight scripting language for PPX
 * 
 * Design Goals:
 * 1. Simple and intuitive syntax inspired by LISP but with verb-first style
 * 2. Seamless integration with PPX infrastructure
 * 3. Support for basic control structures and data types
 * 4. Extensible design for future enhancements
 * 
 * Basic Syntax Example: PolyxScript.md
 * Implementation Notes:
 * - Lexical analysis with token classification
 * - Recursive descent parsing
 * - AST-based interpretation
 * - Dynamic typing system
 * - Simple symbol table for variables
 */

#ifndef PPDB_POLYX_SCRIPT_H
#define PPDB_POLYX_SCRIPT_H
/**
先占位，后面等 ppx通过后再来实现。

参考 LISP，但动词在括号前面；

 */
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxError.h"

// Forward declarations
typedef struct PolyxScript PolyxScript;
typedef struct PolyxScriptClassType PolyxScriptClassType;

// Value types
typedef enum {
    POLYX_VALUE_NULL,
    POLYX_VALUE_NUMBER,
    POLYX_VALUE_STRING,
    POLYX_VALUE_BOOLEAN,
    POLYX_VALUE_FUNCTION,
    POLYX_VALUE_ARRAY,
    POLYX_VALUE_OBJECT,
    POLYX_VALUE_PROMISE
} PolyxValueType;

// Value structure
typedef struct PolyxValue {
    PolyxValueType type;
    union {
        double number;
        char* string;
        InfraxBool boolean;
        struct {
            PolyxAstNode* body;
            char** parameters;
            InfraxSize param_count;
            struct PolyxScope* closure;
        } function;
        struct {
            struct PolyxValue** elements;
            InfraxSize count;
        } array;
        struct {
            char** keys;
            struct PolyxValue** values;
            InfraxSize count;
        } object;
        PolyxPromise promise;
    } as;
} PolyxValue;

// Scope structure for variable management
typedef struct PolyxScope {
    struct PolyxScope* parent;
    char** names;
    PolyxValue** values;
    InfraxSize count;
    InfraxSize capacity;
} PolyxScope;

// Token types
typedef enum {
    POLYX_TOKEN_EOF = 0,
    POLYX_TOKEN_NUMBER,
    POLYX_TOKEN_STRING,
    POLYX_TOKEN_IDENTIFIER,
    POLYX_TOKEN_KEYWORD,
    POLYX_TOKEN_OPERATOR,
    POLYX_TOKEN_PUNCTUATION
} PolyxTokenType;

// Token structure
typedef struct {
    PolyxTokenType type;
    char* value;
    InfraxSize line;
    InfraxSize column;
} PolyxToken;

// AST node types
typedef enum {
    POLYX_AST_NUMBER,
    POLYX_AST_STRING,
    POLYX_AST_IDENTIFIER,
    POLYX_AST_BINARY_OP,
    POLYX_AST_UNARY_OP,
    POLYX_AST_ASSIGNMENT,
    POLYX_AST_IF,
    POLYX_AST_WHILE,
    POLYX_AST_BLOCK
} PolyxAstType;

// AST node structure
typedef struct PolyxAstNode {
    PolyxAstType type;
    union {
        double number_value;
        char* string_value;
        char* identifier;
        struct {
            char operator;
            struct PolyxAstNode* left;
            struct PolyxAstNode* right;
        } binary_op;
        struct {
            char operator;
            struct PolyxAstNode* operand;
        } unary_op;
        struct {
            char* name;
            struct PolyxAstNode* value;
        } assignment;
        struct {
            struct PolyxAstNode* condition;
            struct PolyxAstNode* then_branch;
            struct PolyxAstNode* else_branch;
        } if_stmt;
        struct {
            struct PolyxAstNode* condition;
            struct PolyxAstNode* body;
        } while_stmt;
        struct {
            struct PolyxAstNode** statements;
            InfraxSize count;
        } block;
    } as;
} PolyxAstNode;

// Async operation types
typedef enum {
    POLYX_ASYNC_NONE,
    POLYX_ASYNC_PENDING,
    POLYX_ASYNC_COMPLETED,
    POLYX_ASYNC_ERROR
} PolyxAsyncState;

// Async operation result
typedef struct {
    PolyxAsyncState state;
    PolyxValue* result;
    char* error_message;
} PolyxAsyncResult;

// Async operation callback
typedef void (*PolyxAsyncCallback)(PolyxScript* script, PolyxAsyncResult* result);

// Async operation context
typedef struct {
    PolyxAsyncState state;
    PolyxValue* promise;
    PolyxAsyncCallback callback;
    void* user_data;
    char* error_message;
} PolyxAsyncContext;

// Promise value
typedef struct {
    PolyxAsyncState state;
    PolyxValue* result;
    PolyxValue* then_handler;
    PolyxValue* catch_handler;
    PolyxAsyncContext* context;
} PolyxPromise;

// Script instance structure
struct PolyxScript {
    PolyxScript* self;
    PolyxScriptClassType* klass;
    
    // Lexer state
    const char* source;
    InfraxSize position;
    InfraxSize line;
    InfraxSize column;
    
    // Parser state
    PolyxToken current_token;
    InfraxBool had_error;
    char* error_message;
    
    // Interpreter state
    PolyxScope* global_scope;
    PolyxScope* current_scope;
    PolyxValue* last_result;
    PolyxAsyncContext* current_async;
    InfraxSize async_count;
    InfraxSize async_capacity;
    PolyxAsyncContext** async_operations;
};

// Script class interface
struct PolyxScriptClassType {
    // Constructor & destructor
    PolyxScript* (*new)(void);
    void (*free)(PolyxScript* self);
    
    // Core functionality
    InfraxError (*load_source)(PolyxScript* self, const char* source);
    InfraxError (*run)(PolyxScript* self);
    
    // Lexer functions
    PolyxToken (*get_next_token)(PolyxScript* self);
    
    // Parser functions
    PolyxAstNode* (*parse_program)(PolyxScript* self);
    PolyxAstNode* (*parse_statement)(PolyxScript* self);
    PolyxAstNode* (*parse_expression)(PolyxScript* self);
    
    // AST node management
    PolyxAstNode* (*create_number_node)(double value);
    PolyxAstNode* (*create_string_node)(const char* value);
    PolyxAstNode* (*create_identifier_node)(const char* name);
    PolyxAstNode* (*create_binary_op_node)(char operator, PolyxAstNode* left, PolyxAstNode* right);
    PolyxAstNode* (*create_unary_op_node)(char operator, PolyxAstNode* operand);
    PolyxAstNode* (*create_assignment_node)(const char* name, PolyxAstNode* value);
    PolyxAstNode* (*create_if_node)(PolyxAstNode* condition, PolyxAstNode* then_branch, PolyxAstNode* else_branch);
    PolyxAstNode* (*create_while_node)(PolyxAstNode* condition, PolyxAstNode* body);
    PolyxAstNode* (*create_block_node)(void);
    InfraxError (*add_statement_to_block)(PolyxAstNode* block, PolyxAstNode* statement);
    void (*free_ast_node)(PolyxAstNode* node);
    
    // Interpreter functions
    PolyxValue* (*create_null_value)(void);
    PolyxValue* (*create_number_value)(double number);
    PolyxValue* (*create_string_value)(const char* string);
    PolyxValue* (*create_boolean_value)(InfraxBool boolean);
    PolyxValue* (*create_function_value)(PolyxAstNode* body, char** parameters, InfraxSize param_count, PolyxScope* closure);
    PolyxValue* (*create_array_value)(void);
    PolyxValue* (*create_object_value)(void);
    void (*free_value)(PolyxValue* value);
    
    // Scope management
    PolyxScope* (*create_scope)(PolyxScope* parent);
    void (*free_scope)(PolyxScope* scope);
    InfraxError (*define_variable)(PolyxScope* scope, const char* name, PolyxValue* value);
    InfraxError (*set_variable)(PolyxScope* scope, const char* name, PolyxValue* value);
    PolyxValue* (*get_variable)(PolyxScope* scope, const char* name);
    
    // Debug support
    void (*print_tokens)(PolyxScript* self);
    void (*print_ast)(PolyxScript* self, PolyxAstNode* node);
    void (*print_value)(PolyxScript* self, PolyxValue* value);
    
    // Async operation management
    PolyxValue* (*create_promise)(PolyxScript* self);
    void (*resolve_promise)(PolyxScript* self, PolyxValue* promise, PolyxValue* value);
    void (*reject_promise)(PolyxScript* self, PolyxValue* promise, const char* error);
    void (*update_async)(PolyxScript* self);
    
    // Built-in async functions
    PolyxValue* (*async_sleep)(PolyxScript* self, PolyxValue** args, InfraxSize arg_count);
    PolyxValue* (*async_read_file)(PolyxScript* self, PolyxValue** args, InfraxSize arg_count);
    PolyxValue* (*async_write_file)(PolyxScript* self, PolyxValue** args, InfraxSize arg_count);
    PolyxValue* (*async_http_get)(PolyxScript* self, PolyxValue** args, InfraxSize arg_count);
    PolyxValue* (*async_http_post)(PolyxScript* self, PolyxValue** args, InfraxSize arg_count);
};

// Global class instance
extern PolyxScriptClassType PolyxScriptClass;

#endif /* PPDB_POLYX_SCRIPT_H */ 