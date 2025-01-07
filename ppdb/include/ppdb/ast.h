#ifndef PPDB_AST_H_
#define PPDB_AST_H_

#include "cosmopolitan.h"

// 前向声明
typedef struct ast_env ast_env_t;

// AST节点类型
typedef enum {
    AST_NUMBER,
    AST_SYMBOL,
    AST_LIST,
    AST_LAMBDA,
    AST_IF,
    AST_CALL
} ast_node_type_t;

// AST节点结构
typedef struct ast_node {
    ast_node_type_t type;
    union {
        double number;
        char* symbol;
        struct {
            struct ast_node* params;
            struct ast_node* body;
        } lambda;
        struct {
            struct ast_node* cond;
            struct ast_node* then_branch;
            struct ast_node* else_branch;
        } if_stmt;
        struct {
            struct ast_node* func;
            struct ast_node** args;
            size_t arg_count;
        } call;
    } value;
    struct ast_node* first;
    struct ast_node* next;
} ast_node_t;

// 函数声明
void ast_init(ast_env_t* env);
ast_node_t* ast_parse(const char* input);
ast_node_t* ast_eval_expr(const char* expr, ast_env_t* env);
ast_node_t* ast_eval(ast_node_t* node, ast_env_t* env);
void ast_node_free(ast_node_t* node);

// 节点创建函数
ast_node_t* ast_create_number(double value);
ast_node_t* ast_create_symbol(const char* name);
ast_node_t* ast_create_call(ast_node_t* func, ast_node_t** args, size_t arg_count);

#endif // PPDB_AST_H_ 