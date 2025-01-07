#include "ppdb/ast.h"
#include "ppdb/ast_runtime.h"

// 解析上下文
typedef struct parser_context {
    const char* input;
} parser_context_t;

// 前向声明
static ast_node_t* parse_expr(parser_context_t* ctx);

// 辅助函数
static int is_separator(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == ';';
}

static void skip_separators(parser_context_t* ctx) {
    while (is_separator(*ctx->input)) {
        ctx->input++;
    }
}

// 节点创建函数
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

// 解析函数
static ast_node_t* parse_number(parser_context_t* ctx) {
    char* end;
    double value = strtod(ctx->input, &end);
    if (end == ctx->input) {
        return NULL;
    }
    ctx->input = end;
    return ast_create_number(value);
}

static ast_node_t* parse_symbol(parser_context_t* ctx) {
    const char* start = ctx->input;
    while (*ctx->input && !is_separator(*ctx->input) && *ctx->input != '(' && *ctx->input != ')') {
        ctx->input++;
    }
    if (ctx->input == start) {
        return NULL;
    }
    size_t len = ctx->input - start;
    char* name = malloc(len + 1);
    memcpy(name, start, len);
    name[len] = '\0';
    ast_node_t* node = ast_create_symbol(name);
    free(name);
    return node;
}

static ast_node_t* parse_call(parser_context_t* ctx) {
    if (*ctx->input != '(') {
        return NULL;
    }
    ctx->input++; // 跳过左括号
    skip_separators(ctx);
    
    ast_node_t* func = parse_symbol(ctx);
    if (!func) {
        return NULL;
    }
    
    ast_node_t** args = NULL;
    size_t arg_count = 0;
    size_t arg_capacity = 8;
    args = malloc(arg_capacity * sizeof(ast_node_t*));
    
    while (*ctx->input && *ctx->input != ')') {
        skip_separators(ctx);
        if (*ctx->input == ')') {
            break;
        }
        
        ast_node_t* arg = parse_expr(ctx);
        if (!arg) {
            // 清理内存
            for (size_t i = 0; i < arg_count; i++) {
                ast_node_free(args[i]);
            }
            free(args);
            ast_node_free(func);
            return NULL;
        }
        
        if (arg_count >= arg_capacity) {
            arg_capacity *= 2;
            args = realloc(args, arg_capacity * sizeof(ast_node_t*));
        }
        args[arg_count++] = arg;
        
        skip_separators(ctx);
    }
    
    if (*ctx->input != ')') {
        // 清理内存
        for (size_t i = 0; i < arg_count; i++) {
            ast_node_free(args[i]);
        }
        free(args);
        ast_node_free(func);
        return NULL;
    }
    ctx->input++; // 跳过右括号
    
    ast_node_t* call = ast_create_call(func, args, arg_count);
    return call;
}

static ast_node_t* parse_sequence(parser_context_t* ctx) {
    ast_node_t** exprs = NULL;
    size_t expr_count = 0;
    size_t expr_capacity = 8;
    exprs = malloc(expr_capacity * sizeof(ast_node_t*));
    
    while (*ctx->input) {
        skip_separators(ctx);
        if (!*ctx->input) {
            break;
        }
        
        ast_node_t* expr = parse_expr(ctx);
        if (!expr) {
            // 清理内存
            for (size_t i = 0; i < expr_count; i++) {
                ast_node_free(exprs[i]);
            }
            free(exprs);
            return NULL;
        }
        
        if (expr_count >= expr_capacity) {
            expr_capacity *= 2;
            exprs = realloc(exprs, expr_capacity * sizeof(ast_node_t*));
        }
        exprs[expr_count++] = expr;
        
        skip_separators(ctx);
    }
    
    if (expr_count == 0) {
        free(exprs);
        return NULL;
    }
    
    if (expr_count == 1) {
        ast_node_t* result = exprs[0];
        free(exprs);
        return result;
    }
    
    ast_node_t* seq = ast_create_call(ast_create_symbol("seq"), exprs, expr_count);
    return seq;
}

static ast_node_t* parse_expr(parser_context_t* ctx) {
    skip_separators(ctx);
    
    if (*ctx->input == '(') {
        return parse_call(ctx);
    } else if (isdigit(*ctx->input) || *ctx->input == '-' || *ctx->input == '+') {
        return parse_number(ctx);
    } else {
        return parse_symbol(ctx);
    }
}

// 内置函数
ast_node_t* builtin_seq(ast_node_t** args, size_t arg_count, ast_env_t* env) {
    if (arg_count == 0) {
        return ast_create_number(0);
    }
    
    ast_node_t* result = NULL;
    for (size_t i = 0; i < arg_count; i++) {
        if (result) {
            ast_node_free(result);
        }
        result = ast_eval(args[i], env);
        if (!result) {
            return NULL;
        }
    }
    return result;
}

ast_node_t* builtin_local(ast_node_t** args, size_t arg_count, ast_env_t* env) {
    if (arg_count != 2 || args[0]->type != AST_SYMBOL) {
        return NULL;
    }
    
    ast_node_t* value = ast_eval(args[1], env);
    if (!value) return NULL;
    
    ast_env_define(env, args[0]->value.symbol, value);
    return value;
}

ast_node_t* builtin_if(ast_node_t** args, size_t arg_count, ast_env_t* env) {
    if (arg_count != 3) {
        return NULL;
    }
    
    ast_node_t* cond = ast_eval(args[0], env);
    if (!cond) {
        return NULL;
    }
    
    ast_node_t* result = NULL;
    if (cond->type == AST_NUMBER && cond->value.number != 0) {
        result = ast_eval(args[1], env);
    } else {
        result = ast_eval(args[2], env);
    }
    
    ast_node_free(cond);
    return result;
}

ast_node_t* builtin_while(ast_node_t** args, size_t arg_count, ast_env_t* env) {
    if (arg_count != 2) {
        return NULL;
    }
    
    ast_node_t* result = NULL;
    while (1) {
        ast_node_t* cond = ast_eval(args[0], env);
        if (!cond) {
            return NULL;
        }
        
        if (cond->type != AST_NUMBER || cond->value.number == 0) {
            ast_node_free(cond);
            break;
        }
        ast_node_free(cond);
        
        if (result) {
            ast_node_free(result);
        }
        result = ast_eval(args[1], env);
        if (!result) {
            return NULL;
        }
    }
    
    return result ? result : ast_create_number(0);
}

ast_node_t* builtin_add(ast_node_t** args, size_t arg_count, ast_env_t* env) {
    if (arg_count != 2) return NULL;
    
    ast_node_t* arg1 = ast_eval(args[0], env);
    if (!arg1 || arg1->type != AST_NUMBER) {
        ast_node_free(arg1);
        return NULL;
    }
    
    ast_node_t* arg2 = ast_eval(args[1], env);
    if (!arg2 || arg2->type != AST_NUMBER) {
        ast_node_free(arg1);
        ast_node_free(arg2);
        return NULL;
    }
    
    double result = arg1->value.number + arg2->value.number;
    ast_node_free(arg1);
    ast_node_free(arg2);
    
    return ast_create_number(result);
}

ast_node_t* builtin_sub(ast_node_t** args, size_t arg_count, ast_env_t* env) {
    if (arg_count != 2) return NULL;
    
    ast_node_t* arg1 = ast_eval(args[0], env);
    if (!arg1 || arg1->type != AST_NUMBER) {
        ast_node_free(arg1);
        return NULL;
    }
    
    ast_node_t* arg2 = ast_eval(args[1], env);
    if (!arg2 || arg2->type != AST_NUMBER) {
        ast_node_free(arg1);
        ast_node_free(arg2);
        return NULL;
    }
    
    double result = arg1->value.number - arg2->value.number;
    ast_node_free(arg1);
    ast_node_free(arg2);
    
    return ast_create_number(result);
}

ast_node_t* builtin_mul(ast_node_t** args, size_t arg_count, ast_env_t* env) {
    if (arg_count != 2) {
        return NULL;
    }
    
    ast_node_t* a = ast_eval(args[0], env);
    if (!a) return NULL;
    
    ast_node_t* b = ast_eval(args[1], env);
    if (!b) {
        ast_node_free(a);
        return NULL;
    }
    
    if (a->type != AST_NUMBER || b->type != AST_NUMBER) {
        ast_node_free(a);
        ast_node_free(b);
        return NULL;
    }
    
    double result = a->value.number * b->value.number;
    ast_node_free(a);
    ast_node_free(b);
    
    return ast_create_number(result);
}

ast_node_t* builtin_div(ast_node_t** args, size_t arg_count, ast_env_t* env) {
    if (arg_count != 2) {
        return NULL;
    }
    
    ast_node_t* a = ast_eval(args[0], env);
    if (!a) return NULL;
    
    ast_node_t* b = ast_eval(args[1], env);
    if (!b) {
        ast_node_free(a);
        return NULL;
    }
    
    if (a->type != AST_NUMBER || b->type != AST_NUMBER || b->value.number == 0) {
        ast_node_free(a);
        ast_node_free(b);
        return NULL;
    }
    
    double result = a->value.number / b->value.number;
    ast_node_free(a);
    ast_node_free(b);
    
    return ast_create_number(result);
}

ast_node_t* builtin_eq(ast_node_t** args, size_t arg_count, ast_env_t* env) {
    if (arg_count != 2) return NULL;
    
    ast_node_t* arg1 = ast_eval(args[0], env);
    if (!arg1 || arg1->type != AST_NUMBER) {
        ast_node_free(arg1);
        return NULL;
    }
    
    ast_node_t* arg2 = ast_eval(args[1], env);
    if (!arg2 || arg2->type != AST_NUMBER) {
        ast_node_free(arg1);
        ast_node_free(arg2);
        return NULL;
    }
    
    double result = (arg1->value.number == arg2->value.number) ? 1.0 : 0.0;
    ast_node_free(arg1);
    ast_node_free(arg2);
    
    return ast_create_number(result);
}

// 环境初始化
void ast_init(ast_env_t* env) {
    ast_env_define(env, "seq", ast_create_symbol("seq"));
    ast_env_define(env, "local", ast_create_symbol("local"));
    ast_env_define(env, "if", ast_create_symbol("if"));
    ast_env_define(env, "while", ast_create_symbol("while"));
    ast_env_define(env, "+", ast_create_symbol("+"));
    ast_env_define(env, "-", ast_create_symbol("-"));
    ast_env_define(env, "*", ast_create_symbol("*"));
    ast_env_define(env, "/", ast_create_symbol("/"));
    ast_env_define(env, "=", ast_create_symbol("="));
}

// 主要函数
ast_node_t* ast_parse(const char* input) {
    parser_context_t ctx = {input};
    return parse_sequence(&ctx);
}

ast_node_t* ast_eval_expr(const char* expr, ast_env_t* env) {
    parser_context_t ctx = {.input = expr};
    ast_node_t* node = parse_expr(&ctx);
    if (!node) return NULL;
    
    ast_node_t* result = ast_eval(node, env);
    ast_node_free(node);
    return result;
}

static ast_node_t* eval_call(ast_node_t* node, ast_env_t* env) {
    if (node->type != AST_CALL) return NULL;
    
    // 获取函数
    ast_node_t* func = ast_eval(node->value.call.func, env);
    if (!func) return NULL;
    
    // 如果是内置函数，直接调用
    if (func->type == AST_SYMBOL) {
        const char* name = func->value.symbol;
        ast_node_t* result = NULL;
        
        if (strcmp(name, "+") == 0) result = builtin_add(node->value.call.args, node->value.call.arg_count, env);
        else if (strcmp(name, "-") == 0) result = builtin_sub(node->value.call.args, node->value.call.arg_count, env);
        else if (strcmp(name, "=") == 0) result = builtin_eq(node->value.call.args, node->value.call.arg_count, env);
        else if (strcmp(name, "local") == 0) result = builtin_local(node->value.call.args, node->value.call.arg_count, env);
        else if (strcmp(name, "if") == 0) result = builtin_if(node->value.call.args, node->value.call.arg_count, env);
        else if (strcmp(name, "lambda") == 0) {
            // 创建一个新的 lambda 函数
            if (node->value.call.arg_count != 2) return NULL;
            ast_node_t* lambda = malloc(sizeof(ast_node_t));
            lambda->type = AST_LAMBDA;
            lambda->value.lambda.params = node->value.call.args[0];
            lambda->value.lambda.body = node->value.call.args[1];
            result = lambda;
        }
        
        ast_node_free(func);
        return result;
    }
    
    // 如果是 lambda 函数，创建新环境并执行
    if (func->type == AST_LAMBDA) {
        // 创建新环境
        ast_env_t* new_env = ast_env_new(env);
        
        // 绑定参数
        if (func->value.lambda.params->type == AST_SYMBOL) {
            // 单个参数
            if (node->value.call.arg_count != 1) {
                ast_env_free(new_env);
                ast_node_free(func);
                return NULL;
            }
            ast_node_t* arg_value = ast_eval(node->value.call.args[0], env);
            if (!arg_value) {
                ast_env_free(new_env);
                ast_node_free(func);
                return NULL;
            }
            ast_env_define(new_env, func->value.lambda.params->value.symbol, arg_value);
        }
        
        // 执行函数体
        ast_node_t* result = ast_eval(func->value.lambda.body, new_env);
        ast_env_free(new_env);
        ast_node_free(func);
        return result;
    }
    
    ast_node_free(func);
    return NULL;
}

ast_node_t* ast_eval(ast_node_t* node, ast_env_t* env) {
    if (!node) return NULL;
    
    switch (node->type) {
        case AST_NUMBER:
            return ast_create_number(node->value.number);
            
        case AST_SYMBOL:
            return ast_env_lookup(env, node->value.symbol);
            
        case AST_CALL:
            return eval_call(node, env);
            
        case AST_LAMBDA:
            // Lambda 函数直接返回自身的副本
            {
                ast_node_t* lambda = malloc(sizeof(ast_node_t));
                lambda->type = AST_LAMBDA;
                lambda->value.lambda = node->value.lambda;
                return lambda;
            }
            
        default:
            return NULL;
    }
}

void ast_node_free(ast_node_t* node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_SYMBOL:
            free(node->value.symbol);
            break;
            
        case AST_CALL:
            ast_node_free(node->value.call.func);
            for (size_t i = 0; i < node->value.call.arg_count; i++) {
                ast_node_free(node->value.call.args[i]);
            }
            free(node->value.call.args);
            break;
            
        default:
            break;
    }
    
    free(node);
} 