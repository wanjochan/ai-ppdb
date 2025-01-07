/*
 * AST (Abstract Syntax Tree) 实现脚本化
 * 
 * 目标: 实现一个表达式解释运行器（为了自建迷你高效脚本用在ppdb数据库）
 * 
 * 要求： 先parse后eval
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
 */

#include "cosmopolitan.h"

//-----------------------------------------------------------------------------
// 基础数据结构
//-----------------------------------------------------------------------------
typedef enum {
    TOK_EOF, TOK_NUMBER, TOK_SYMBOL, TOK_LPAREN, TOK_RPAREN, TOK_COMMA
} TokenType;

typedef struct {
    TokenType type;
    union {
        double num;
        char sym[32];
    } value;
} Token;

typedef struct {
    const char* input;
    size_t pos;
    Token current;
    bool has_current;
} Parser;

// AST节点类型
typedef enum {
    NODE_NUMBER,     // 数字字面量
    NODE_SYMBOL,     // 符号引用
    NODE_IF,         // if表达式
    NODE_WHILE,      // while循环
    NODE_LOCAL,      // 局部变量定义
    NODE_LAMBDA,     // lambda函数定义
    NODE_CALL,       // 函数调用
    NODE_SEQUENCE    // 语句序列
} NodeType;

// AST节点结构
typedef struct Node {
    NodeType type;
    union {
        double num;                  // NODE_NUMBER
        char sym[32];               // NODE_SYMBOL
        struct {                    // NODE_IF
            struct Node* cond;
            struct Node* then_expr;
            struct Node* else_expr;
        } if_expr;
        struct {                    // NODE_WHILE
            struct Node* cond;
            struct Node* body;
        } while_expr;
        struct {                    // NODE_LOCAL
            char name[32];
            struct Node* value;
        } local;
        struct {                    // NODE_LAMBDA
            struct Node** params;    // 动态数组
            size_t param_count;
            struct Node* body;
            struct Env* closure;     // 捕获的环境
        } lambda;
        struct {                    // NODE_CALL
            struct Node* func;
            struct Node** args;      // 动态数组
            size_t arg_count;
        } call;
        struct {                    // NODE_SEQUENCE
            struct Node** exprs;     // 动态数组
            size_t expr_count;
        } sequence;
    } data;
    struct Node* next;              // 用于链表
} Node;

// 运行时值类型
typedef enum {
    VAL_NUMBER,
    VAL_SYMBOL,
    VAL_LAMBDA,
    VAL_NIL,
    VAL_ERROR
} ValueType;

// 运行时值
typedef struct Value {
    ValueType type;
    union {
        double num;
        char sym[32];
        struct {
            Node* lambda_node;
            struct Env* closure;
        } lambda;
        struct {
            char message[256];
        } error;
    } data;
} Value;

// 环境表项
typedef struct EnvEntry {
    char name[32];
    Value value;
    struct EnvEntry* next;
} EnvEntry;

// 环境
typedef struct Env {
    struct Env* parent;
    EnvEntry* entries;  // 哈希表的每个桶是一个链表
    size_t size;       // 当前环境中的绑定数量
} Env;

// 前向声明
static Token next_token(Parser* p);
static Node* parse_expr(Parser* p);
static Node* parse_number(Parser* p, Token tok);
static Node* parse_symbol(Parser* p, Token tok);
static Node* parse_if(Parser* p);
static Node* parse_while(Parser* p);
static Node* parse_local(Parser* p);
static Node* parse_lambda(Parser* p);
static Node* parse_call(Parser* p, Node* func);
static Value eval(Node* node, Env* env);
static void free_node(Node* node);
static Env* new_env(Env* parent);
static void free_env(Env* env);
static Value* env_get(Env* env, const char* name);
static bool env_set(Env* env, const char* name, Value value);

static Token peek_token(Parser* p) {
    if (!p->has_current) {
        p->current = next_token(p);
        p->has_current = true;
    }
    return p->current;
}

static Token consume_token(Parser* p) {
    Token tok;
    if (p->has_current) {
        tok = p->current;
        p->has_current = false;
    } else {
        tok = next_token(p);
    }
    return tok;
}

//-----------------------------------------------------------------------------
// 词法分析
//-----------------------------------------------------------------------------
static Token next_token(Parser* p) {
    Token tok = {0};
    
    // 跳过空白
    while (p->input[p->pos] && p->input[p->pos] <= ' ')
        p->pos++;
        
    if (!p->input[p->pos]) {
        tok.type = TOK_EOF;
        return tok;
    }
    
    // 解析数字
    if (isdigit(p->input[p->pos]) || p->input[p->pos] == '-') {
        tok.type = TOK_NUMBER;
        char* endptr;
        tok.value.num = strtod(p->input + p->pos, &endptr);
        p->pos += endptr - (p->input + p->pos);
        return tok;
    }
    
    // 解析括号和逗号
    char c = p->input[p->pos];
    if (c == '(' || c == ')' || c == ',') {
        p->pos++;
        switch (c) {
            case '(': tok.type = TOK_LPAREN; break;
            case ')': tok.type = TOK_RPAREN; break;
            case ',': tok.type = TOK_COMMA; break;
        }
        return tok;
    }
    
    // 解析符号
    if (isalpha(c) || strchr("+-*/=<>!", c)) {
        tok.type = TOK_SYMBOL;
        size_t i = 0;
        while (p->input[p->pos] && !isspace(p->input[p->pos]) && 
               p->input[p->pos] != '(' && p->input[p->pos] != ')' && 
               p->input[p->pos] != ',') {
            if (i < sizeof(tok.value.sym)-1)
                tok.value.sym[i++] = p->input[p->pos];
            p->pos++;
        }
        tok.value.sym[i] = '\0';
        return tok;
    }
    
    p->pos++;  // 跳过未知字符
    return tok;
}

//-----------------------------------------------------------------------------
// 内存管理
//-----------------------------------------------------------------------------
static Node* new_node(NodeType type) {
    Node* node = (Node*)calloc(1, sizeof(Node));
    if (node) node->type = type;
    return node;
}

static void free_node(Node* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_IF:
            free_node(node->data.if_expr.cond);
            free_node(node->data.if_expr.then_expr);
            free_node(node->data.if_expr.else_expr);
            break;
        case NODE_WHILE:
            free_node(node->data.while_expr.cond);
            free_node(node->data.while_expr.body);
            break;
        case NODE_LOCAL:
            free_node(node->data.local.value);
            break;
        case NODE_LAMBDA:
            for (size_t i = 0; i < node->data.lambda.param_count; i++)
                free_node(node->data.lambda.params[i]);
            free(node->data.lambda.params);
            free_node(node->data.lambda.body);
            break;
        case NODE_CALL:
            free_node(node->data.call.func);
            for (size_t i = 0; i < node->data.call.arg_count; i++)
                free_node(node->data.call.args[i]);
            free(node->data.call.args);
            break;
        case NODE_SEQUENCE:
            for (size_t i = 0; i < node->data.sequence.expr_count; i++)
                free_node(node->data.sequence.exprs[i]);
            free(node->data.sequence.exprs);
            break;
    }
    free(node);
}

//-----------------------------------------------------------------------------
// 环境管理
//-----------------------------------------------------------------------------
static Env* new_env(Env* parent) {
    Env* env = (Env*)calloc(1, sizeof(Env));
    if (env) env->parent = parent;
    return env;
}

static void free_env(Env* env) {
    if (!env) return;
    
    EnvEntry* entry = env->entries;
    while (entry) {
        EnvEntry* next = entry->next;
        free(entry);
        entry = next;
    }
    free(env);
}

static Value* env_get(Env* env, const char* name) {
    while (env) {
        EnvEntry* entry = env->entries;
        while (entry) {
            if (strcmp(entry->name, name) == 0)
                return &entry->value;
            entry = entry->next;
        }
        env = env->parent;
    }
    return NULL;
}

static bool env_set(Env* env, const char* name, Value value) {
    EnvEntry* entry = (EnvEntry*)malloc(sizeof(EnvEntry));
    if (!entry) return false;
    
    strncpy(entry->name, name, sizeof(entry->name)-1);
    entry->value = value;
    entry->next = env->entries;
    env->entries = entry;
    env->size++;
    return true;
}

//-----------------------------------------------------------------------------
// 解析器
//-----------------------------------------------------------------------------
static Node* parse_expr(Parser* p) {
    Token tok = consume_token(p);
    Node* node = NULL;
    
    switch (tok.type) {
        case TOK_NUMBER:
            node = parse_number(p, tok);
            break;
            
        case TOK_SYMBOL: {
            // 检查是否是特权符号或运算符
            if (strcmp(tok.value.sym, "if") == 0) {
                Token next = peek_token(p);
                if (next.type != TOK_LPAREN) break;
                consume_token(p);  // 消费左括号
                node = parse_if(p);
            }
            else if (strcmp(tok.value.sym, "while") == 0) {
                Token next = peek_token(p);
                if (next.type != TOK_LPAREN) break;
                consume_token(p);  // 消费左括号
                node = parse_while(p);
            }
            else if (strcmp(tok.value.sym, "local") == 0) {
                Token next = peek_token(p);
                if (next.type != TOK_LPAREN) break;
                consume_token(p);  // 消费左括号
                node = parse_local(p);
            }
            else if (strcmp(tok.value.sym, "lambda") == 0) {
                Token next = peek_token(p);
                if (next.type != TOK_LPAREN) break;
                consume_token(p);  // 消费左括号
                node = parse_lambda(p);
            }
            else if (strcmp(tok.value.sym, "+") == 0 || 
                     strcmp(tok.value.sym, "-") == 0 ||
                     strcmp(tok.value.sym, "*") == 0 ||
                     strcmp(tok.value.sym, "/") == 0) {
                Token next = peek_token(p);
                if (next.type != TOK_LPAREN) break;
                consume_token(p);  // 消费左括号
                Node* func = parse_symbol(p, tok);
                if (!func) break;
                node = parse_call(p, func);
            }
            else {
                node = parse_symbol(p, tok);
                // 检查是否是函数调用
                Token next = peek_token(p);
                if (next.type == TOK_LPAREN) {
                    consume_token(p);  // 消费左括号
                    Node* func = node;
                    node = parse_call(p, func);
                    if (!node) free_node(func);
                }
            }
            break;
        }
            
        case TOK_LPAREN: {
            tok = consume_token(p);
            if (tok.type != TOK_SYMBOL) break;
            
            // 解析函数调用
            Node* func = parse_symbol(p, tok);
            if (!func) break;
            
            node = parse_call(p, func);
            break;
        }
    }
    
    return node;
}

static Node* parse_number(Parser* p, Token tok) {
    Node* node = new_node(NODE_NUMBER);
    if (node) node->data.num = tok.value.num;
    return node;
}

static Node* parse_symbol(Parser* p, Token tok) {
    Node* node = new_node(NODE_SYMBOL);
    if (node) strncpy(node->data.sym, tok.value.sym, sizeof(node->data.sym)-1);
    return node;
}

static Node* parse_if(Parser* p) {
    Node* node = new_node(NODE_IF);
    if (!node) return NULL;
    
    // 解析条件
    node->data.if_expr.cond = parse_expr(p);
    if (!node->data.if_expr.cond) goto error;
    
    if (next_token(p).type != TOK_COMMA) goto error;
    
    // 解析then分支
    node->data.if_expr.then_expr = parse_expr(p);
    if (!node->data.if_expr.then_expr) goto error;
    
    if (next_token(p).type != TOK_COMMA) goto error;
    
    // 解析else分支
    node->data.if_expr.else_expr = parse_expr(p);
    if (!node->data.if_expr.else_expr) goto error;
    
    if (next_token(p).type != TOK_RPAREN) goto error;
    
    return node;
    
error:
    free_node(node);
    return NULL;
}

static Node* parse_while(Parser* p) {
    Node* node = new_node(NODE_WHILE);
    if (!node) return NULL;
    
    // 解析条件
    node->data.while_expr.cond = parse_expr(p);
    if (!node->data.while_expr.cond) goto error;
    
    if (next_token(p).type != TOK_COMMA) goto error;
    
    // 解析循环体
    node->data.while_expr.body = parse_expr(p);
    if (!node->data.while_expr.body) goto error;
    
    if (next_token(p).type != TOK_RPAREN) goto error;
    
    return node;
    
error:
    free_node(node);
    return NULL;
}

static Node* parse_local(Parser* p) {
    Node* node = new_node(NODE_LOCAL);
    if (!node) return NULL;
    
    // 解析变量名
    Token name = next_token(p);
    if (name.type != TOK_SYMBOL) goto error;
    strncpy(node->data.local.name, name.value.sym, sizeof(node->data.local.name)-1);
    
    // 检查第一个逗号
    if (next_token(p).type != TOK_COMMA) goto error;
    
    // 解析值
    node->data.local.value = parse_expr(p);
    if (!node->data.local.value) goto error;
    
    // 检查右括号或逗号
    Token tok = next_token(p);
    if (tok.type == TOK_COMMA) {
        // 如果还有更多表达式，创建一个序列节点
        Node* seq = new_node(NODE_SEQUENCE);
        if (!seq) goto error;
        
        // 分配初始大小为2的数组
        seq->data.sequence.exprs = (Node**)calloc(2, sizeof(Node*));
        if (!seq->data.sequence.exprs) {
            free_node(seq);
            goto error;
        }
        
        // 添加第一个表达式（当前的local节点）
        seq->data.sequence.exprs[0] = node;
        seq->data.sequence.expr_count = 1;
        
        // 解析下一个表达式
        Node* next = parse_expr(p);
        if (!next) {
            free_node(seq);
            goto error;
        }
        
        // 添加第二个表达式
        seq->data.sequence.exprs[1] = next;
        seq->data.sequence.expr_count = 2;
        
        // 检查最后的右括号
        if (next_token(p).type != TOK_RPAREN) {
            free_node(seq);
            goto error;
        }
        
        return seq;
    }
    
    if (tok.type != TOK_RPAREN) goto error;
    return node;
    
error:
    free_node(node);
    return NULL;
}

static Node* parse_lambda(Parser* p) {
    Node* node = new_node(NODE_LAMBDA);
    if (!node) return NULL;
    
    // 预分配参数数组
    node->data.lambda.params = NULL;
    node->data.lambda.param_count = 0;
    
    // 解析参数列表
    Token tok = consume_token(p);
    if (tok.type != TOK_SYMBOL) goto error;
    
    // 创建参数节点
    Node* param = new_node(NODE_SYMBOL);
    if (!param) goto error;
    strncpy(param->data.sym, tok.value.sym, sizeof(param->data.sym)-1);
    
    // 分配参数数组
    node->data.lambda.params = (Node**)malloc(sizeof(Node*));
    if (!node->data.lambda.params) {
        free_node(param);
        goto error;
    }
    node->data.lambda.params[0] = param;
    node->data.lambda.param_count = 1;
    
    // 检查逗号
    tok = consume_token(p);
    if (tok.type != TOK_COMMA) goto error;
    
    // 解析函数体
    node->data.lambda.body = parse_expr(p);
    if (!node->data.lambda.body) goto error;
    
    // 检查右括号
    tok = consume_token(p);
    if (tok.type != TOK_RPAREN) goto error;
    
    return node;
    
error:
    free_node(node);
    return NULL;
}

static Node* parse_call(Parser* p, Node* func) {
    Node* node = new_node(NODE_CALL);
    if (!node) return NULL;
    
    node->data.call.func = func;
    node->data.call.args = NULL;
    node->data.call.arg_count = 0;
    
    // 解析参数列表
    Node* arg = parse_expr(p);
    if (!arg) goto error;
    
    // 分配参数数组
    node->data.call.args = (Node**)malloc(sizeof(Node*));
    if (!node->data.call.args) {
        free_node(arg);
        goto error;
    }
    node->data.call.args[0] = arg;
    node->data.call.arg_count = 1;
    
    // 检查右括号
    Token tok = consume_token(p);
    if (tok.type != TOK_RPAREN) goto error;
    
    return node;
    
error:
    free_node(node);
    return NULL;
}

//-----------------------------------------------------------------------------
// 求值器
//-----------------------------------------------------------------------------
static Value eval_if(Node* node, Env* env) {
    Value cond = eval(node->data.if_expr.cond, env);
    if (cond.type != VAL_NUMBER)
        return (Value){.type = VAL_ERROR, .data.error.message = "Condition must be a number"};
    
    return eval(cond.data.num ? node->data.if_expr.then_expr : node->data.if_expr.else_expr, env);
}

static Value eval_while(Node* node, Env* env) {
    Value result = {.type = VAL_NIL};
    
    while (1) {
        Value cond = eval(node->data.while_expr.cond, env);
        if (cond.type != VAL_NUMBER)
            return (Value){.type = VAL_ERROR, .data.error.message = "Condition must be a number"};
        
        if (cond.data.num == 0) break;
        
        result = eval(node->data.while_expr.body, env);
        if (result.type == VAL_ERROR) return result;
    }
    
    return result;
}

static Value eval_local(Node* node, Env* env) {
    Value value = eval(node->data.local.value, env);
    if (value.type == VAL_ERROR) return value;
    
    if (!env_set(env, node->data.local.name, value))
        return (Value){.type = VAL_ERROR, .data.error.message = "Failed to set local variable"};
    
    return value;
}

static Value eval_lambda(Node* node, Env* env) {
    Value lambda = {.type = VAL_LAMBDA};
    lambda.data.lambda.lambda_node = node;
    lambda.data.lambda.closure = env;  // 捕获当前环境
    return lambda;
}

static Value eval_call(Node* node, Env* env) {
    // 求值函数
    Value func = eval(node->data.call.func, env);
    if (func.type != VAL_LAMBDA)
        return (Value){.type = VAL_ERROR, .data.error.message = "Not a function"};
    
    // 创建新环境
    Env* call_env = new_env(func.data.lambda.closure);
    if (!call_env)
        return (Value){.type = VAL_ERROR, .data.error.message = "Out of memory"};
    
    // 求值并绑定参数
    Node* lambda = func.data.lambda.lambda_node;
    if (node->data.call.arg_count != lambda->data.lambda.param_count) {
        free_env(call_env);
        return (Value){.type = VAL_ERROR, .data.error.message = "Wrong number of arguments"};
    }
    
    for (size_t i = 0; i < node->data.call.arg_count; i++) {
        Value arg = eval(node->data.call.args[i], env);
        if (arg.type == VAL_ERROR) {
            free_env(call_env);
            return arg;
        }
        
        Node* param = lambda->data.lambda.params[i];
        if (!env_set(call_env, param->data.sym, arg)) {
            free_env(call_env);
            return (Value){.type = VAL_ERROR, .data.error.message = "Failed to set parameter"};
        }
    }
    
    // 求值函数体
    Value result = eval(lambda->data.lambda.body, call_env);
    free_env(call_env);
    return result;
}

static Value eval_add(Node* node, Env* env) {
    if (node->type != NODE_CALL || node->data.call.arg_count != 2)
        return (Value){.type = VAL_ERROR, .data.error.message = "Add requires 2 arguments"};
    
    Value a = eval(node->data.call.args[0], env);
    if (a.type != VAL_NUMBER)
        return (Value){.type = VAL_ERROR, .data.error.message = "Add requires number arguments"};
    
    Value b = eval(node->data.call.args[1], env);
    if (b.type != VAL_NUMBER)
        return (Value){.type = VAL_ERROR, .data.error.message = "Add requires number arguments"};
    
    return (Value){.type = VAL_NUMBER, .data.num = a.data.num + b.data.num};
}

static Value eval_sub(Node* node, Env* env) {
    if (node->type != NODE_CALL || node->data.call.arg_count != 2)
        return (Value){.type = VAL_ERROR, .data.error.message = "Sub requires 2 arguments"};
    
    Value a = eval(node->data.call.args[0], env);
    if (a.type != VAL_NUMBER)
        return (Value){.type = VAL_ERROR, .data.error.message = "Sub requires number arguments"};
    
    Value b = eval(node->data.call.args[1], env);
    if (b.type != VAL_NUMBER)
        return (Value){.type = VAL_ERROR, .data.error.message = "Sub requires number arguments"};
    
    return (Value){.type = VAL_NUMBER, .data.num = a.data.num - b.data.num};
}

static Value eval_mul(Node* node, Env* env) {
    if (node->type != NODE_CALL || node->data.call.arg_count != 2)
        return (Value){.type = VAL_ERROR, .data.error.message = "Mul requires 2 arguments"};
    
    Value a = eval(node->data.call.args[0], env);
    if (a.type != VAL_NUMBER)
        return (Value){.type = VAL_ERROR, .data.error.message = "Mul requires number arguments"};
    
    Value b = eval(node->data.call.args[1], env);
    if (b.type != VAL_NUMBER)
        return (Value){.type = VAL_ERROR, .data.error.message = "Mul requires number arguments"};
    
    return (Value){.type = VAL_NUMBER, .data.num = a.data.num * b.data.num};
}

static Value eval_div(Node* node, Env* env) {
    if (node->type != NODE_CALL || node->data.call.arg_count != 2)
        return (Value){.type = VAL_ERROR, .data.error.message = "Div requires 2 arguments"};
    
    Value a = eval(node->data.call.args[0], env);
    if (a.type != VAL_NUMBER)
        return (Value){.type = VAL_ERROR, .data.error.message = "Div requires number arguments"};
    
    Value b = eval(node->data.call.args[1], env);
    if (b.type != VAL_NUMBER || b.data.num == 0)
        return (Value){.type = VAL_ERROR, .data.error.message = "Div requires non-zero number arguments"};
    
    return (Value){.type = VAL_NUMBER, .data.num = a.data.num / b.data.num};
}

static Value eval(Node* node, Env* env) {
    if (!node) return (Value){.type = VAL_NIL};
    
    switch (node->type) {
        case NODE_NUMBER:
            return (Value){.type = VAL_NUMBER, .data.num = node->data.num};
            
        case NODE_SYMBOL: {
            Value* val = env_get(env, node->data.sym);
            if (!val)
                return (Value){.type = VAL_ERROR, .data.error.message = "Undefined symbol"};
            return *val;
        }
            
        case NODE_IF:
            return eval_if(node, env);
            
        case NODE_WHILE:
            return eval_while(node, env);
            
        case NODE_LOCAL:
            return eval_local(node, env);
            
        case NODE_LAMBDA:
            return eval_lambda(node, env);
            
        case NODE_CALL: {
            // 检查是否是内置运算符
            if (node->data.call.func->type == NODE_SYMBOL) {
                const char* op = node->data.call.func->data.sym;
                if (strcmp(op, "+") == 0) return eval_add(node, env);
                if (strcmp(op, "-") == 0) return eval_sub(node, env);
                if (strcmp(op, "*") == 0) return eval_mul(node, env);
                if (strcmp(op, "/") == 0) return eval_div(node, env);
            }
            return eval_call(node, env);
        }
            
        case NODE_SEQUENCE: {
            Value result = {.type = VAL_NIL};
            for (size_t i = 0; i < node->data.sequence.expr_count; i++) {
                result = eval(node->data.sequence.exprs[i], env);
                if (result.type == VAL_ERROR) return result;
            }
            return result;
        }
    }
    
    return (Value){.type = VAL_NIL};
}

//-----------------------------------------------------------------------------
// 主入口
//-----------------------------------------------------------------------------
static void print_value(Value v) {
    switch (v.type) {
        case VAL_NUMBER:
            printf("%g", v.data.num);
            break;
        case VAL_LAMBDA:
            printf("<lambda>");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_ERROR:
            printf("Error: %s", v.data.error.message);
            break;
    }
}

Value eval_expr(const char* input) {
    Parser parser = {
        .input = input,
        .pos = 0,
        .has_current = false
    };
    Node* ast = parse_expr(&parser);
    if (!ast) return (Value){.type = VAL_ERROR, .data.error.message = "Parse error"};
    
    Env* env = new_env(NULL);
    if (!env) {
        free_node(ast);
        return (Value){.type = VAL_ERROR, .data.error.message = "Out of memory"};
    }
    
    Value result = eval(ast, env);
    
    free_node(ast);
    free_env(env);
    return result;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <expr>\n", argv[0]);
        return 1;
    }
    
    Value result = eval_expr(argv[1]);
    print_value(result);
    printf("\n");
    return result.type == VAL_ERROR ? 1 : 0;
}

// ... existing code ... 