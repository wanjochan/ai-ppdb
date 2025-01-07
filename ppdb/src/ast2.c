/*
 * 简化版脚本解释器
 * 
 * 设计目标：
 * 1. 先parse后eval
 * 2. 最小内存占用
 * 3. 高效执行
 * 4. 代码简洁
 */

#include "cosmopolitan.h"

//-----------------------------------------------------------------------------
// 类型定义
//-----------------------------------------------------------------------------

// 节点类型
typedef enum {
    NODE_NUM,    // 数字
    NODE_SYM,    // 符号
    NODE_IF,     // if表达式
    NODE_CALL,   // 函数调用
    NODE_LOCAL,  // 局部变量
    NODE_LAMBDA  // lambda函数
} NodeType;

// 值类型
typedef enum {
    VAL_NIL,  // 空值
    VAL_NUM,  // 数字
    VAL_FUN,  // 函数
    VAL_ERR   // 错误
} ValueType;

// 前向声明
struct Node;
struct Env;

// 函数值
typedef struct {
    struct Node* node;  // lambda节点
    struct Env* env;    // 捕获的环境
} Function;

// 值
typedef struct {
    ValueType type;
    union {
        double num;       // VAL_NUM
        Function fun;     // VAL_FUN
        const char* err;  // VAL_ERR
    } data;
} Value;

// 节点
typedef struct Node {
    NodeType type;
    union {
        double num;  // NODE_NUM
        char sym[32];  // NODE_SYM
        struct {  // NODE_IF
            struct Node* cond;
            struct Node* then_expr;
            struct Node* else_expr;
        } if_expr;
        struct {  // NODE_CALL
            char name[32];
            struct Node* args[8];
            size_t arg_count;
        } call;
        struct {  // NODE_LOCAL
            char name[32];
            struct Node* value;
            struct Node* next;
        } local;
        struct {  // NODE_LAMBDA
            char param[32];
            struct Node* body;
        } lambda;
    } data;
} Node;

// 环境变量
typedef struct {
    char name[32];
    Value value;
} EnvVar;

// 环境
typedef struct Env {
    struct Env* parent;
    EnvVar vars[32];
    size_t ref_count;  // 引用计数
} Env;

// 解析器
typedef struct {
    const char* cur;
} Parser;

// 常量
#define MAX_VARS 32
#define MAX_ARGS 8
#define MAX_IDENT 32

//-----------------------------------------------------------------------------
// 前向声明
//-----------------------------------------------------------------------------

static Node* new_node(NodeType type);
static void free_node(Node* node);
static Value eval_call(Node* node, Env* env);
static Value eval(Node* node, Env* env);
static Value builtin_add(Node* node, Env* env);
static Value builtin_sub(Node* node, Env* env);
static Node* parse_expr(Parser* p);

//-----------------------------------------------------------------------------
// 工具函数
//-----------------------------------------------------------------------------

// 跳过空白字符
static void skip_space(Parser* p) {
    while (isspace(*p->cur)) p->cur++;
}

// 检查当前字符
static bool match(Parser* p, char c) {
    skip_space(p);
    if (*p->cur != c) return false;
    p->cur++;
    return true;
}

// 解析标识符
static bool parse_ident(Parser* p, char* buf, size_t size) {
    skip_space(p);
    
    if (!isalpha(*p->cur) && *p->cur != '_' && !strchr("+-*/=<>!", *p->cur))
        return false;
        
    size_t i = 0;
    while (i < size - 1 && (isalnum(*p->cur) || *p->cur == '_' || strchr("+-*/=<>!", *p->cur))) {
        buf[i++] = *p->cur++;
    }
    buf[i] = '\0';
    return true;
}

// 解析数字
static bool parse_number_token(Parser* p, double* num) {
    skip_space(p);
    
    char* end;
    *num = strtod(p->cur, &end);
    if (end == p->cur) return false;
    
    p->cur = end;
    return true;
}

// 解析数字节点
static Node* parse_number(Parser* p) {
    double num;
    if (!parse_number_token(p, &num)) return NULL;
    
    Node* node = new_node(NODE_NUM);
    if (node) node->data.num = num;
    return node;
}

//-----------------------------------------------------------------------------
// 内存管理
//-----------------------------------------------------------------------------

// 创建新节点
static Node* new_node(NodeType type) {
    Node* node = (Node*)calloc(1, sizeof(Node));
    if (node) node->type = type;
    return node;
}

// 释放节点
static void free_node(Node* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_IF:
            free_node(node->data.if_expr.cond);
            free_node(node->data.if_expr.then_expr);
            free_node(node->data.if_expr.else_expr);
            break;
        case NODE_CALL:
            for (size_t i = 0; i < node->data.call.arg_count; i++)
                free_node(node->data.call.args[i]);
            break;
        case NODE_LOCAL:
            free_node(node->data.local.value);
            free_node(node->data.local.next);
            break;
        case NODE_LAMBDA:
            free_node(node->data.lambda.body);
            break;
    }
    free(node);
}

//-----------------------------------------------------------------------------
// 环境管理
//-----------------------------------------------------------------------------

// 创建新环境
static Env* new_env(Env* parent) {
    Env* env = (Env*)calloc(1, sizeof(Env));
    if (env) {
        env->parent = parent;
        // 如果有父环境,增加引用计数
        if (parent) parent->ref_count++;
    }
    return env;
}

// 释放环境
static void free_env(Env* env) {
    if (!env) return;
    
    // 减少引用计数
    if (env->ref_count > 0) {
        env->ref_count--;
        return;
    }
    
    // 释放父环境
    if (env->parent) free_env(env->parent);
    
    // 释放本环境
    free(env);
}

// 添加变量到环境
static bool env_add(Env* env, const char* name, Value value) {
    if (!env || !name) return false;
    
    for (size_t i = 0; i < MAX_VARS; i++) {
        if (!env->vars[i].name[0]) {
            strncpy(env->vars[i].name, name, sizeof(env->vars[i].name) - 1);
            env->vars[i].value = value;
            return true;
        }
    }
    return false;
}

// 从环境中获取变量
static Value* env_get(Env* env, const char* name) {
    if (!env || !name) return NULL;
    
    // 先在当前环境中查找
    for (size_t i = 0; i < MAX_VARS; i++) {
        if (env->vars[i].name[0] && strcmp(env->vars[i].name, name) == 0)
            return &env->vars[i].value;
    }
    
    // 如果没找到,在父环境中查找
    return env->parent ? env_get(env->parent, name) : NULL;
}

//-----------------------------------------------------------------------------
// 解析器
//-----------------------------------------------------------------------------

// 解析参数列表
static bool parse_args(Parser* p, Node** args, size_t* count) {
    *count = 0;
    
    while (*count < MAX_ARGS) {
        Node* arg = parse_expr(p);
        if (!arg) break;
        
        args[(*count)++] = arg;
        
        if (!match(p, ',')) break;
    }
    
    return true;
}

// 解析函数调用
static Node* parse_call(Parser* p) {
    Node* node = new_node(NODE_CALL);
    if (!node) return NULL;
    
    // 解析函数名
    if (!parse_ident(p, node->data.call.name, sizeof(node->data.call.name))) {
        free_node(node);
        return NULL;
    }
    
    // 解析左括号
    if (!match(p, '(')) {
        free_node(node);
        return NULL;
    }
    
    // 解析参数列表
    node->data.call.arg_count = 0;
    while (*p->cur && *p->cur != ')') {
        if (node->data.call.arg_count > 0) {
            if (!match(p, ',')) {
                free_node(node);
                return NULL;
            }
        }
        
        Node* arg = parse_expr(p);
        if (!arg) {
            free_node(node);
            return NULL;
        }
        
        if (node->data.call.arg_count >= MAX_ARGS) {
            free_node(arg);
            free_node(node);
            return NULL;
        }
        
        node->data.call.args[node->data.call.arg_count++] = arg;
    }
    
    // 解析右括号
    if (!match(p, ')')) {
        free_node(node);
        return NULL;
    }
    
    return node;
}

// 解析标识符或函数调用
static Node* parse_ident_or_call(Parser* p) {
    char name[MAX_IDENT];
    if (!parse_ident(p, name, sizeof(name))) return NULL;
    
    skip_space(p);
    if (*p->cur == '(')
        return parse_call(p);
        
    Node* node = new_node(NODE_SYM);
    if (node) strncpy(node->data.sym, name, sizeof(node->data.sym) - 1);
    return node;
}

// 解析if表达式
static Node* parse_if(Parser* p) {
    if (!match(p, '(')) return NULL;
    
    Node* node = new_node(NODE_IF);
    if (!node) return NULL;
    
    // 解析条件
    node->data.if_expr.cond = parse_expr(p);
    if (!node->data.if_expr.cond) goto error;
    
    if (!match(p, ',')) goto error;
    
    // 解析then分支
    node->data.if_expr.then_expr = parse_expr(p);
    if (!node->data.if_expr.then_expr) goto error;
    
    if (!match(p, ',')) goto error;
    
    // 解析else分支
    node->data.if_expr.else_expr = parse_expr(p);
    if (!node->data.if_expr.else_expr) goto error;
    
    if (!match(p, ')')) goto error;
    
    return node;
    
error:
    free_node(node);
    return NULL;
}

// 解析local定义
static Node* parse_local(Parser* p) {
    if (!match(p, '(')) return NULL;
    
    Node* node = new_node(NODE_LOCAL);
    if (!node) return NULL;
    
    // 解析变量名
    if (!parse_ident(p, node->data.local.name, sizeof(node->data.local.name)))
        goto error;
    
    if (!match(p, ',')) goto error;
    
    // 解析值
    node->data.local.value = parse_expr(p);
    if (!node->data.local.value) goto error;
    
    // 检查是否有后续表达式
    if (match(p, ',')) {
        node->data.local.next = parse_expr(p);
        if (!node->data.local.next) goto error;
    }
    
    if (!match(p, ')')) goto error;
    
    return node;
    
error:
    free_node(node);
    return NULL;
}

// 解析lambda函数
static Node* parse_lambda(Parser* p) {
    if (!match(p, '(')) return NULL;
    
    Node* node = new_node(NODE_LAMBDA);
    if (!node) return NULL;
    
    // 解析参数名
    if (!parse_ident(p, node->data.lambda.param, sizeof(node->data.lambda.param)))
        goto error;
    
    if (!match(p, ',')) goto error;
    
    // 解析函数体
    node->data.lambda.body = parse_expr(p);
    if (!node->data.lambda.body) goto error;
    
    if (!match(p, ')')) goto error;
    
    return node;
    
error:
    free_node(node);
    return NULL;
}

// 解析表达式
static Node* parse_expr(Parser* p) {
    skip_space(p);
    
    // 解析数字
    double num;
    if (parse_number_token(p, &num)) {
        Node* node = new_node(NODE_NUM);
        if (node) node->data.num = num;
        return node;
    }
    
    // 解析标识符
    char name[MAX_IDENT];
    if (parse_ident(p, name, sizeof(name))) {
        // 检查是否是特殊形式
        if (strcmp(name, "if") == 0) {
            if (!match(p, '(')) return NULL;
            Node* node = new_node(NODE_IF);
            if (!node) return NULL;
            
            node->data.if_expr.cond = parse_expr(p);
            if (!node->data.if_expr.cond || !match(p, ',')) {
                free_node(node);
                return NULL;
            }
            
            node->data.if_expr.then_expr = parse_expr(p);
            if (!node->data.if_expr.then_expr || !match(p, ',')) {
                free_node(node);
                return NULL;
            }
            
            node->data.if_expr.else_expr = parse_expr(p);
            if (!node->data.if_expr.else_expr || !match(p, ')')) {
                free_node(node);
                return NULL;
            }
            
            return node;
        }
        else if (strcmp(name, "local") == 0) {
            if (!match(p, '(')) return NULL;
            Node* node = new_node(NODE_LOCAL);
            if (!node) return NULL;
            
            if (!parse_ident(p, node->data.local.name, sizeof(node->data.local.name)) || !match(p, ',')) {
                free_node(node);
                return NULL;
            }
            
            node->data.local.value = parse_expr(p);
            if (!node->data.local.value) {
                free_node(node);
                return NULL;
            }
            
            if (match(p, ',')) {
                node->data.local.next = parse_expr(p);
                if (!node->data.local.next) {
                    free_node(node);
                    return NULL;
                }
            }
            
            if (!match(p, ')')) {
                free_node(node);
                return NULL;
            }
            
            return node;
        }
        else if (strcmp(name, "lambda") == 0) {
            if (!match(p, '(')) return NULL;
            Node* node = new_node(NODE_LAMBDA);
            if (!node) return NULL;
            
            if (!parse_ident(p, node->data.lambda.param, sizeof(node->data.lambda.param)) || !match(p, ',')) {
                free_node(node);
                return NULL;
            }
            
            node->data.lambda.body = parse_expr(p);
            if (!node->data.lambda.body || !match(p, ')')) {
                free_node(node);
                return NULL;
            }
            
            return node;
        }
        
        // 解析函数调用
        Node* node = new_node(NODE_CALL);
        if (!node) return NULL;
        
        strncpy(node->data.call.name, name, sizeof(node->data.call.name) - 1);
        
        if (!match(p, '(')) {
            free_node(node);
            Node* sym = new_node(NODE_SYM);
            if (sym) strncpy(sym->data.sym, name, sizeof(sym->data.sym) - 1);
            return sym;
        }
        
        // 解析参数列表
        node->data.call.arg_count = 0;
        while (*p->cur && *p->cur != ')') {
            if (node->data.call.arg_count > 0) {
                if (!match(p, ',')) {
                    free_node(node);
                    return NULL;
                }
            }
            
            Node* arg = parse_expr(p);
            if (!arg) {
                free_node(node);
                return NULL;
            }
            
            if (node->data.call.arg_count >= MAX_ARGS) {
                free_node(arg);
                free_node(node);
                return NULL;
            }
            
            node->data.call.args[node->data.call.arg_count++] = arg;
        }
        
        if (!match(p, ')')) {
            free_node(node);
            return NULL;
        }
        
        return node;
    }
    
    return NULL;
}

//-----------------------------------------------------------------------------
// 求值器
//-----------------------------------------------------------------------------

// 内置加法函数
static Value builtin_add(Node* node, Env* env) {
    if (!node || !env || node->data.call.arg_count != 2)
        return (Value){.type = VAL_ERR, .data.err = "Add requires 2 arguments"};
    
    Value arg1 = eval(node->data.call.args[0], env);
    if (arg1.type == VAL_ERR) return arg1;
    
    Value arg2 = eval(node->data.call.args[1], env);
    if (arg2.type == VAL_ERR) return arg2;
    
    if (arg1.type != VAL_NUM || arg2.type != VAL_NUM)
        return (Value){.type = VAL_ERR, .data.err = "Arguments must be numbers"};
    
    return (Value){.type = VAL_NUM, .data.num = arg1.data.num + arg2.data.num};
}

// 内置减法函数
static Value builtin_sub(Node* node, Env* env) {
    if (!node || !env || node->data.call.arg_count != 2)
        return (Value){.type = VAL_ERR, .data.err = "Sub requires 2 arguments"};
    
    Value arg1 = eval(node->data.call.args[0], env);
    if (arg1.type == VAL_ERR) return arg1;
    
    Value arg2 = eval(node->data.call.args[1], env);
    if (arg2.type == VAL_ERR) return arg2;
    
    if (arg1.type != VAL_NUM || arg2.type != VAL_NUM)
        return (Value){.type = VAL_ERR, .data.err = "Arguments must be numbers"};
    
    return (Value){.type = VAL_NUM, .data.num = arg1.data.num - arg2.data.num};
}

// 求值函数调用
static Value eval_call(Node* node, Env* env) {
    if (!node || !env) return (Value){.type = VAL_ERR, .data.err = "Invalid call"};
    
    // 检查是否是内置函数
    if (strcmp(node->data.call.name, "+") == 0) return builtin_add(node, env);
    if (strcmp(node->data.call.name, "-") == 0) return builtin_sub(node, env);
    
    // 获取用户定义函数
    Value* fun = env_get(env, node->data.call.name);
    if (!fun) return (Value){.type = VAL_ERR, .data.err = "Function not found"};
    if (fun->type != VAL_FUN) return (Value){.type = VAL_ERR, .data.err = "Not a function"};
    
    // 创建新环境,父环境是函数定义时的环境
    Env* env_new = new_env(fun->data.fun.env);
    if (!env_new) return (Value){.type = VAL_ERR, .data.err = "Out of memory"};
    
    // 计算参数值并绑定到新环境
    if (node->data.call.arg_count > 0) {
        Value arg = eval(node->data.call.args[0], env);
        if (arg.type == VAL_ERR) {
            free_env(env_new);
            return arg;
        }
        if (!env_add(env_new, fun->data.fun.node->data.lambda.param, arg)) {
            free_env(env_new);
            return (Value){.type = VAL_ERR, .data.err = "Failed to bind parameter"};
        }
    }
    
    // 执行函数体
    Value result = eval(fun->data.fun.node->data.lambda.body, env_new);
    free_env(env_new);
    return result;
}

// 修改eval_lambda函数
static Value eval_lambda(Node* node, Env* env) {
    if (!node || !env) return (Value){.type = VAL_ERR, .data.err = "Invalid lambda"};
    
    // 返回函数值,直接使用当前环境
    Value lambda = {
        .type = VAL_FUN,
        .data.fun = {
            .node = node,
            .env = env  // 不再创建新环境,直接使用当前环境
        }
    };
    return lambda;
}

// 修改eval_local函数
static Value eval_local(Node* node, Env* env) {
    if (!node || !env) return (Value){.type = VAL_ERR, .data.err = "Invalid local"};
    
    // 求值初始值
    Value value = eval(node->data.local.value, env);
    if (value.type == VAL_ERR) return value;
    
    // 先绑定变量,这样递归函数可以引用自己
    if (!env_add(env, node->data.local.name, value))
        return (Value){.type = VAL_ERR, .data.err = "Failed to add local variable"};
    
    // 如果是lambda函数,更新其环境为当前环境
    if (value.type == VAL_FUN) {
        value.data.fun.env = env;
        env->ref_count++;  // 增加环境引用计数
        if (!env_add(env, node->data.local.name, value))
            return (Value){.type = VAL_ERR, .data.err = "Failed to update function environment"};
    }
    
    // 如果有后续表达式,继续求值
    if (node->data.local.next) {
        Value next_value = eval(node->data.local.next, env);
        if (next_value.type == VAL_ERR) return next_value;
        return next_value;
    }
        
    return value;
}

// 求值if表达式
static Value eval_if(Node* node, Env* env) {
    if (!node || !env) return (Value){.type = VAL_ERR, .data.err = "Invalid if"};
    
    // 求值条件
    Value cond = eval(node->data.if_expr.cond, env);
    if (cond.type == VAL_ERR) return cond;
    
    if (cond.type != VAL_NUM)
        return (Value){.type = VAL_ERR, .data.err = "Condition must be a number"};
    
    // 根据条件选择分支
    return eval(cond.data.num ? node->data.if_expr.then_expr : node->data.if_expr.else_expr, env);
}

// 主求值函数
static Value eval(Node* node, Env* env) {
    if (!node || !env) return (Value){.type = VAL_ERR, .data.err = "Invalid expression"};
    
    switch (node->type) {
        case NODE_NUM:
            return (Value){.type = VAL_NUM, .data.num = node->data.num};
            
        case NODE_SYM: {
            Value* val = env_get(env, node->data.sym);
            if (!val) return (Value){.type = VAL_ERR, .data.err = "Undefined variable"};
            return *val;
        }
        
        case NODE_IF:
            return eval_if(node, env);
            
        case NODE_CALL:
            return eval_call(node, env);
            
        case NODE_LOCAL:
            return eval_local(node, env);
            
        case NODE_LAMBDA:
            return eval_lambda(node, env);
    }
    
    return (Value){.type = VAL_ERR, .data.err = "Invalid node type"};
}

//-----------------------------------------------------------------------------
// 主函数
//-----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <expression>\n", argv[0]);
        return 1;
    }
    
    // 初始化解析器
    Parser parser = {argv[1]};
    
    // 解析表达式
    Node* node = parse_expr(&parser);
    if (!node) {
        fprintf(stderr, "Parse error\n");
        return 1;
    }
    
    // 创建全局环境
    Env* env = new_env(NULL);
    if (!env) {
        fprintf(stderr, "Out of memory\n");
        free_node(node);
        return 1;
    }
    
    // 求值表达式
    Value result = eval(node, env);
    
    // 输出结果
    switch (result.type) {
        case VAL_NIL:
            printf("nil\n");
            break;
        case VAL_NUM:
            printf("%g\n", result.data.num);
            break;
        case VAL_FUN:
            printf("<lambda>\n");
            break;
        case VAL_ERR:
            fprintf(stderr, "%s\n", result.data.err);
            break;
    }
    
    // 清理资源
    free_node(node);
    free_env(env);
    
    return result.type == VAL_ERR ? 1 : 0;
} 