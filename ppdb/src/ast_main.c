#include "ppdb/ast.h"
#include "cosmopolitan.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <expression>\n", argv[0]);
        return 1;
    }
    
    // 初始化AST环境
    ast_init();
    
    // 解析并求值表达式
    ast_node_t *result = ast_eval_expr(argv[1]);
    if (!result) {
        fprintf(stderr, "Error: Failed to evaluate expression\n");
        return 1;
    }
    
    // 打印结果
    switch (result->type) {
        case AST_NUMBER:
            printf("NUMBER(%g)\n", result->value.number_value);
            break;
            
        case AST_SYMBOL:
            printf("SYMBOL(%s)\n", result->value.symbol.name);
            break;
            
        case AST_CALL: {
            printf("CALL(%s", result->value.call.func->value.symbol.name);
            for (size_t i = 0; i < result->value.call.arg_count; i++) {
                printf(", ");
                switch (result->value.call.args[i]->type) {
                    case AST_NUMBER:
                        printf("%g", result->value.call.args[i]->value.number_value);
                        break;
                    case AST_SYMBOL:
                        printf("%s", result->value.call.args[i]->value.symbol.name);
                        break;
                    case AST_CALL:
                        printf("...");
                        break;
                }
            }
            printf(")\n");
            break;
        }
    }
    
    ast_free(result);
    return 0;
} 