/*
 * AST (Abstract Syntax Tree) 实现脚本化
 * 
 * 目标: 实现一个表达式解释运行其（为了自建迷你高效脚本用在ppdb数据库）
 * 
 * 要求： 边parse便eval
 * 语法糖:
 * 0. 支持顺序执行
 * 1. 运行时特权symbol：
 *    - if: 条件分支， if(?,true_expr,false_expr)
 *    - while: 循环, while(?,loop_expr)
 *    - local: 在当前函数调用栈层定义或赋值变量  local(var_name, var_expr)
 *    - lambda(params_expr, body_expr) 定义函数
 * 2 函数eval: symbol(arg1, arg2, ...)
 *    - 参数之间用逗号或空格分隔
 * 3. 演示 symbol:
 *    - add/sub/mul/div: 加减乘除，同时支持缩写 +,-,*,/
 *    - eq: 相等比较，同时支持缩写==
 * 
 * 测试（简单）：
 * cd ppdb/scripts/; .\build_ast.bat
 */

#include "cosmopolitan.h"
#include "ppdb/ast.h"

// 构造函数
ast_node_t* ast_create_number(double value) {
    ast_node_t* node = malloc(sizeof(ast_node_t));
    node->type = AST_NUMBER;
    node->value.number = value;
    node->first = NULL;
    node->next = NULL;
    return node;
}

ast_node_t* ast_create_symbol(const char* name) {
    ast_node_t* node = malloc(sizeof(ast_node_t));
    node->type = AST_SYMBOL;
    node->value.symbol = strdup(name);
    node->first = NULL;
    node->next = NULL;
    return node;
}

ast_node_t* ast_create_list(void) {
    ast_node_t* node = malloc(sizeof(ast_node_t));
    node->type = AST_LIST;
    node->first = NULL;
    node->next = NULL;
    return node;
}

ast_node_t* ast_create_lambda(ast_node_t* params, ast_node_t* body) {
    ast_node_t* node = malloc(sizeof(ast_node_t));
    node->type = AST_LAMBDA;
    node->value.lambda.params = params;
    node->value.lambda.body = body;
    node->first = NULL;
    node->next = NULL;
    return node;
}

ast_node_t* ast_create_if(ast_node_t* cond, ast_node_t* then_branch, ast_node_t* else_branch) {
    ast_node_t* node = malloc(sizeof(ast_node_t));
    node->type = AST_IF;
    node->value.if_stmt.cond = cond;
    node->value.if_stmt.then_branch = then_branch;
    node->value.if_stmt.else_branch = else_branch;
    node->first = NULL;
    node->next = NULL;
    return node;
}

ast_node_t* ast_create_call(ast_node_t* func, ast_node_t** args, size_t arg_count) {
    ast_node_t* node = malloc(sizeof(ast_node_t));
    node->type = AST_CALL;
    node->value.call.func = func;
    node->value.call.args = args;
    node->value.call.arg_count = arg_count;
    node->first = NULL;
    node->next = NULL;
    return node;
}

void ast_node_free(ast_node_t* node) {
    if (!node) return;
    
    // 防止重复释放
    if (node->type == AST_INVALID) return;
    
    switch (node->type) {
        case AST_SYMBOL:
            if (node->value.symbol) {
                free(node->value.symbol);
                node->value.symbol = NULL;
            }
            break;
        case AST_LAMBDA:
            if (node->value.lambda.params) {
                ast_node_free(node->value.lambda.params);
                node->value.lambda.params = NULL;
            }
            if (node->value.lambda.body) {
                ast_node_free(node->value.lambda.body);
                node->value.lambda.body = NULL;
            }
            break;
        case AST_IF:
            if (node->value.if_stmt.cond) {
                ast_node_free(node->value.if_stmt.cond);
                node->value.if_stmt.cond = NULL;
            }
            if (node->value.if_stmt.then_branch) {
                ast_node_free(node->value.if_stmt.then_branch);
                node->value.if_stmt.then_branch = NULL;
            }
            if (node->value.if_stmt.else_branch) {
                ast_node_free(node->value.if_stmt.else_branch);
                node->value.if_stmt.else_branch = NULL;
            }
            break;
        case AST_CALL:
            if (node->value.call.func) {
                ast_node_free(node->value.call.func);
                node->value.call.func = NULL;
            }
            if (node->value.call.args) {
                for (size_t i = 0; i < node->value.call.arg_count; i++) {
                    if (node->value.call.args[i]) {
                        ast_node_free(node->value.call.args[i]);
                        node->value.call.args[i] = NULL;
                    }
                }
                free(node->value.call.args);
                node->value.call.args = NULL;
            }
            break;
        default:
            break;
    }
    
    if (node->first) {
        ast_node_free(node->first);
        node->first = NULL;
    }
    if (node->next) {
        ast_node_free(node->next);
        node->next = NULL;
    }
    
    // 标记为已释放
    node->type = AST_INVALID;
    free(node);
}

ast_node_t* ast_node_copy(ast_node_t* node) {
    if (!node) return NULL;
    
    ast_node_t* copy = malloc(sizeof(ast_node_t));
    if (!copy) return NULL;
    
    copy->type = node->type;
    copy->first = NULL;
    copy->next = NULL;
    
    switch (node->type) {
        case AST_NUMBER:
            copy->value.number = node->value.number;
            break;
            
        case AST_SYMBOL:
            copy->value.symbol = strdup(node->value.symbol);
            if (!copy->value.symbol) {
                free(copy);
                return NULL;
            }
            break;
            
        case AST_LAMBDA:
            copy->value.lambda.params = ast_node_copy(node->value.lambda.params);
            copy->value.lambda.body = ast_node_copy(node->value.lambda.body);
            if (!copy->value.lambda.params || !copy->value.lambda.body) {
                ast_node_free(copy);
                return NULL;
            }
            break;
            
        case AST_IF:
            copy->value.if_stmt.cond = ast_node_copy(node->value.if_stmt.cond);
            copy->value.if_stmt.then_branch = ast_node_copy(node->value.if_stmt.then_branch);
            copy->value.if_stmt.else_branch = ast_node_copy(node->value.if_stmt.else_branch);
            if (!copy->value.if_stmt.cond || !copy->value.if_stmt.then_branch) {
                ast_node_free(copy);
                return NULL;
            }
            break;
            
        case AST_CALL:
            copy->value.call.func = ast_node_copy(node->value.call.func);
            if (!copy->value.call.func) {
                ast_node_free(copy);
                return NULL;
            }
            
            copy->value.call.arg_count = node->value.call.arg_count;
            copy->value.call.args = malloc(sizeof(ast_node_t*) * node->value.call.arg_count);
            if (!copy->value.call.args) {
                ast_node_free(copy);
                return NULL;
            }
            
            for (size_t i = 0; i < node->value.call.arg_count; i++) {
                copy->value.call.args[i] = ast_node_copy(node->value.call.args[i]);
                if (!copy->value.call.args[i]) {
                    ast_node_free(copy);
                    return NULL;
                }
            }
            break;
            
        case AST_LIST:
            if (node->first) {
                copy->first = ast_node_copy(node->first);
                if (!copy->first) {
                    ast_node_free(copy);
                    return NULL;
                }
                
                ast_node_t* src = node->first->next;
                ast_node_t* dst = copy->first;
                
                while (src) {
                    dst->next = ast_node_copy(src);
                    if (!dst->next) {
                        ast_node_free(copy);
                        return NULL;
                    }
                    dst = dst->next;
                    src = src->next;
                }
            }
            break;
            
        default:
            free(copy);
            return NULL;
    }
    
    return copy;
} 


int main(int argc, char *argv[]) {
    //ast_env_t* env = ast_env_new(NULL);
    //print_result(ast_eval_expr(argv[1], env));
    //ast_env_free(env);
    //TODO 目前支持三种特权symbol： if/while/local/lambda，其他的用local+lambda方式初始化
    print_result(eval_expr(argv[1]))
    return 0;
} 