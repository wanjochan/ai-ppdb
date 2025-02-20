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

// Forward declarations
typedef struct PolyxScript PolyxScript;
typedef struct PolyxScriptClassType PolyxScriptClassType;

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
    
    // Interpreter state
    struct {
        char** names;
        double* values;
        InfraxSize count;
        InfraxSize capacity;
    } variables;
};

// Script class interface
struct PolyxScriptClassType {
    // Constructor & destructor
    PolyxScript* (*new)(void);
    void (*free)(PolyxScript* self);
    
    // Core functionality
    InfraxError (*load_source)(PolyxScript* self, const char* source);
    InfraxError (*run)(PolyxScript* self);
    
    // Debug support
    void (*print_tokens)(PolyxScript* self);
    void (*print_ast)(PolyxScript* self, PolyxAstNode* node);
};

// Global class instance
extern PolyxScriptClassType PolyxScriptClass;

#endif /* PPDB_POLYX_SCRIPT_H */ 