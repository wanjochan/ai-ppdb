#include "ppdb/ast.h"
#include "ppdb/ast_runtime.h"
#include "cosmopolitan.h"

static void print_result(ast_node_t *result) {
    if (!result) {
        printf("Error: evaluation failed\n");
        return;
    }
    
    switch (result->type) {
        case AST_NUMBER:
            printf("%g\n", result->value.number);
            break;
            
        case AST_SYMBOL:
            printf("%s\n", result->value.symbol);
            break;
            
        case AST_LAMBDA:
            printf("<lambda>\n");
            break;
            
        case AST_CALL:
            printf("Error: unexpected call result\n");
            break;
            
        default:
            printf("<unknown type: %d>\n", result->type);
            break;
    }
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <expression>\n", argv[0]);
        return 1;
    }
    
    // 创建并初始化 AST 环境
    ast_env_t* env = ast_env_new(NULL);
    ast_init(env);
    
    // 解析并求值表达式
    ast_node_t *result = ast_eval_expr(argv[1], env);
    print_result(result);
    
    // 清理资源
    ast_node_free(result);
    ast_env_free(env);
    
    return 0;
} 