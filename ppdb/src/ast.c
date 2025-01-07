#include "ppdb/ast.h"
#include "ppdb/ast_runtime.h"
#include "cosmopolitan.h"

/* Parser context */
typedef struct {
    const char *input;
    size_t position;
    size_t length;
} parser_context_t;

/* Parser utilities */
static bool is_whitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static bool is_alnum(char c) { return is_alpha(c) || is_digit(c); }

static void skip_whitespace(parser_context_t *ctx) {
    while (ctx->position < ctx->length && is_whitespace(ctx->input[ctx->position])) {
        ctx->position++;
    }
}

/* Basic parsers */
static ast_node_t *parse_number(parser_context_t *ctx) {
    char *end;
    double value = strtod(ctx->input + ctx->position, &end);
    if (end == ctx->input + ctx->position) return NULL;
    ctx->position = end - ctx->input;
    return ast_create_number(value);
}

static ast_node_t *parse_symbol(parser_context_t *ctx) {
    if (!is_alpha(ctx->input[ctx->position])) return NULL;
    size_t start = ctx->position;
    while (ctx->position < ctx->length && is_alnum(ctx->input[ctx->position])) ctx->position++;
    size_t length = ctx->position - start;
    char *name = malloc(length + 1);
    if (!name) return NULL;
    memcpy(name, ctx->input + start, length);
    name[length] = '\0';
    ast_node_t *node = ast_create_symbol(name);
    free(name);
    return node;
}

/* Function call parser */
static ast_node_t *parse_call(parser_context_t *ctx) {
    size_t start_pos = ctx->position;
    
    // 解析函数名
    ast_node_t *func = parse_symbol(ctx);
    if (!func) return NULL;
    
    skip_whitespace(ctx);
    
    // 检查左括号
    if (ctx->position >= ctx->length || ctx->input[ctx->position] != '(') {
        ast_free(func);
        ctx->position = start_pos;
        return NULL;
    }
    ctx->position++;
    
    // 解析参数列表
    ast_node_t *args[16];  // 最多支持16个参数
    size_t arg_count = 0;
    
    skip_whitespace(ctx);
    while (ctx->position < ctx->length && ctx->input[ctx->position] != ')') {
        if (arg_count > 0) {
            if (ctx->input[ctx->position] != ',') {
                for (size_t i = 0; i < arg_count; i++) ast_free(args[i]);
                ast_free(func);
                ctx->position = start_pos;
                return NULL;
            }
            ctx->position++;
            skip_whitespace(ctx);
        }
        
        ast_node_t *arg = parse_number(ctx);
        if (!arg) arg = parse_symbol(ctx);
        if (!arg) {
            for (size_t i = 0; i < arg_count; i++) ast_free(args[i]);
            ast_free(func);
            ctx->position = start_pos;
            return NULL;
        }
        
        if (arg_count >= 16) {
            ast_free(arg);
            for (size_t i = 0; i < arg_count; i++) ast_free(args[i]);
            ast_free(func);
            ctx->position = start_pos;
            return NULL;
        }
        
        args[arg_count++] = arg;
        skip_whitespace(ctx);
    }
    
    // 检查右括号
    if (ctx->position >= ctx->length || ctx->input[ctx->position] != ')') {
        for (size_t i = 0; i < arg_count; i++) ast_free(args[i]);
        ast_free(func);
        ctx->position = start_pos;
        return NULL;
    }
    ctx->position++;
    
    // 创建函数调用节点
    ast_node_t *call = ast_create_call(func, args, arg_count);
    if (!call) {
        for (size_t i = 0; i < arg_count; i++) ast_free(args[i]);
        ast_free(func);
    }
    return call;
}

/* Expression parser */
static ast_node_t *parse_expr(const char *input) {
    parser_context_t ctx = {input, 0, strlen(input)};
    skip_whitespace(&ctx);
    
    ast_node_t *result = parse_call(&ctx);
    if (result) return result;
    
    result = parse_number(&ctx);
    if (result) return result;
    
    return parse_symbol(&ctx);
}

/* Built-in functions */
static ast_node_t *builtin_local(ast_node_t **args, size_t arg_count) {
    if (arg_count != 2 || args[0]->type != AST_SYMBOL) return NULL;
    env_define(args[0]->value.symbol.name, args[1]);
    return ast_clone(args[1]);
}

static ast_node_t *builtin_if(ast_node_t **args, size_t arg_count) {
    if (arg_count != 3) return NULL;
    ast_node_t *cond = ast_eval(args[0]);
    if (!cond || cond->type != AST_NUMBER) {
        ast_free(cond);
        return NULL;
    }
    size_t branch = cond->value.number_value != 0 ? 1 : 2;
    ast_free(cond);
    return ast_eval(args[branch]);
}

static ast_node_t *builtin_while(ast_node_t **args, size_t arg_count) {
    if (arg_count != 2) return NULL;
    ast_node_t *last_result = NULL;
    while (1) {
        ast_node_t *cond = ast_eval(args[0]);
        if (!cond || cond->type != AST_NUMBER) {
            ast_free(cond);
            ast_free(last_result);
            return NULL;
        }
        if (cond->value.number_value == 0) {
            ast_free(cond);
            return last_result ? last_result : ast_create_number(0);
        }
        ast_free(cond);
        ast_free(last_result);
        last_result = ast_eval(args[1]);
        if (!last_result) return NULL;
    }
}

/* Public interface */
void ast_init(void) {
    // 注入local函数
    ast_node_t *local = ast_create_symbol("local");
    if (local) {
        env_define("local", local);
        ast_free(local);
    }
    
    // 注入if函数
    ast_node_t *if_sym = ast_create_symbol("if");
    if (if_sym) {
        env_define("if", if_sym);
        ast_free(if_sym);
    }
    
    // 注入while函数
    ast_node_t *while_sym = ast_create_symbol("while");
    if (while_sym) {
        env_define("while", while_sym);
        ast_free(while_sym);
    }
}

ast_node_t *ast_eval_expr(const char *input) {
    ast_node_t *expr = parse_expr(input);
    if (!expr) return NULL;
    
    ast_node_t *result = ast_eval(expr);
    ast_free(expr);
    return result;
} 