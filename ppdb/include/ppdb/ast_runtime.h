#ifndef PPDB_AST_RUNTIME_H_
#define PPDB_AST_RUNTIME_H_

#include "cosmopolitan.h"
#include "ppdb/ast.h"

// 环境条目结构
typedef struct env_entry {
    char* name;
    ast_node_t* value;
    struct env_entry* next;
} env_entry_t;

// 环境结构
typedef struct ast_env {
    env_entry_t* entries;
    struct ast_env* parent;
} ast_env_t;

// 环境函数
ast_env_t* ast_env_new(ast_env_t* parent);
void ast_env_free(ast_env_t* env);
void ast_env_define(ast_env_t* env, const char* name, ast_node_t* value);
ast_node_t* ast_env_lookup(ast_env_t* env, const char* name);

// 内置函数
ast_node_t* builtin_seq(ast_node_t** args, size_t arg_count, ast_env_t* env);
ast_node_t* builtin_local(ast_node_t** args, size_t arg_count, ast_env_t* env);
ast_node_t* builtin_if(ast_node_t** args, size_t arg_count, ast_env_t* env);
ast_node_t* builtin_while(ast_node_t** args, size_t arg_count, ast_env_t* env);
ast_node_t* builtin_add(ast_node_t** args, size_t arg_count, ast_env_t* env);
ast_node_t* builtin_sub(ast_node_t** args, size_t arg_count, ast_env_t* env);
ast_node_t* builtin_mul(ast_node_t** args, size_t arg_count, ast_env_t* env);
ast_node_t* builtin_div(ast_node_t** args, size_t arg_count, ast_env_t* env);
ast_node_t* builtin_eq(ast_node_t** args, size_t arg_count, ast_env_t* env);

#endif // PPDB_AST_RUNTIME_H_ 