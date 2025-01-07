#ifndef PPDB_AST_H_
#define PPDB_AST_H_

#include "ppdb/ast_runtime.h"

/* Initialize AST environment and inject built-in functions */
void ast_init(void);

/* Parse and evaluate an expression */
ast_node_t *ast_eval_expr(const char *input);

#endif /* PPDB_AST_H_ */ 