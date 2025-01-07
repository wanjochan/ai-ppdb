#ifndef PPDB_AST_RUNTIME_H_
#define PPDB_AST_RUNTIME_H_

/* AST node types */
typedef enum {
    AST_SYMBOL,
    AST_NUMBER,
    AST_CALL
} ast_node_type_t;

/* AST node structure */
typedef struct ast_node {
    ast_node_type_t type;
    union {
        double number_value;
        struct { char *name; } symbol;
        struct {
            struct ast_node *func;
            struct ast_node **args;
            size_t arg_count;
        } call;
    } value;
} ast_node_t;

/* Runtime AST functions */
ast_node_t *ast_create_number(double value);
ast_node_t *ast_create_symbol(const char *name);
ast_node_t *ast_create_call(ast_node_t *func, ast_node_t **args, size_t arg_count);
void ast_free(ast_node_t *node);
ast_node_t *ast_clone(ast_node_t *node);
ast_node_t *ast_eval(ast_node_t *node);

/* Runtime environment functions */
void env_define(const char *name, ast_node_t *value);
ast_node_t *env_lookup(const char *name);

#endif /* PPDB_AST_RUNTIME_H_ */ 