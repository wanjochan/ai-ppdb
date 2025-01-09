//-----------------------------------------------------------------------------
// AST implementation v3
//-----------------------------------------------------------------------------

#include "cosmopolitan.h"

//-----------------------------------------------------------------------------
// 常量定义
//-----------------------------------------------------------------------------

#define MAX_STR 256
#define MAX_IDENT 64
#define MAX_VARS 64
#define MAX_ARGS 16
#define MAX_LIST 16
#define MAX_DEPTH 64

//-----------------------------------------------------------------------------
// 类型定义
//-----------------------------------------------------------------------------

// 节点类型
typedef enum {
    NODE_NUM,
    NODE_SYM,
    NODE_STR,
    NODE_BOOL,
    NODE_LIST,
    NODE_IF,
    NODE_CALL,
    NODE_LOCAL,
    NODE_LAMBDA
} NodeType;

// 值类型
typedef enum {
    VAL_NUM,
    VAL_STR,
    VAL_BOOL,
    VAL_LIST,
    VAL_FUN,
    VAL_ERR
} ValueType;

// 符号类型
typedef enum {
    SYM_BUILTIN,
    SYM_SPECIAL
} SymbolType;

typedef struct Node Node;
typedef struct Env Env;
typedef struct Value Value;
typedef struct Parser Parser;

// 列表
typedef struct {
    Node* items[MAX_LIST];
    size_t count;
} List;

// 函数
typedef struct {
    Node* node;
    Env* env;
} Fun;

// 值
struct Value {
    ValueType type;
    union {
        double num;
        char str[MAX_STR];
        bool boolean;
        List list;
        Fun fun;
        char err[MAX_STR];
    } data;
};

// 函数类型
typedef union {
    Value (*builtin)(Node*, Env*);
    Value (*special)(Node*, Env*);
} Function;

// 符号表项
typedef struct {
    const char* name;
    SymbolType type;
    Function func;
} SymbolEntry;

// 环境变量
typedef struct {
    char name[MAX_IDENT];
    Value value;
} EnvVar;

// 环境
struct Env {
    EnvVar vars[MAX_VARS];
    size_t var_count;
    Env* parent;
    size_t depth;
};

// 节点
struct Node {
    NodeType type;
    union {
        double num;
        char sym[MAX_IDENT];
        char str[MAX_STR];
        bool boolean;
        List list;
        struct {
            Node* cond;
            Node* then_expr;
            Node* else_expr;
        } if_expr;
        struct {
            char name[MAX_IDENT];
            Node* args[MAX_ARGS];
            size_t arg_count;
        } call;
        struct {
            char name[MAX_IDENT];
            Node* value;
            Node* next;
        } local;
        struct {
            char param[MAX_IDENT];
            Node* body;
        } lambda;
    } data;
};

// 解析器
struct Parser {
    const char* cur;
    size_t line;
    size_t column;
};

// 前向声明
static bool env_add(Env* env, const char* name, Value value);
static Value* env_get(Env* env, const char* name);
static bool is_whitespace(char c);
static bool is_digit(char c);
static bool is_alpha(char c);
static bool is_symbol(char c);
static void skip_whitespace(Parser* p);
static Node* parse_number(Parser* p);
static Node* parse_symbol(Parser* p);
static Node* parse_string(Parser* p);
static Node* parse_call(Parser* p);
static Node* parse_if(Parser* p);
static Node* parse_lambda(Parser* p);
static Node* parse_local(Parser* p);
static Node* parse_expr(Parser* p);
static Value eval(Node* node, Env* env);
static Value eval_if(Node* node, Env* env);
static Value eval_local(Node* node, Env* env);
static Value eval_lambda(Node* node, Env* env);
static Node* alloc_node(void);
static Value add(Node* node, Env* env);
static Value multiply(Node* node, Env* env);
static Value divide(Node* node, Env* env);
static Value modulo(Node* node, Env* env);
static SymbolEntry* lookup_symbol(const char* name);

//-----------------------------------------------------------------------------
// 内置函数表
//-----------------------------------------------------------------------------

static SymbolEntry builtin_symbols[] = {
    {"+", SYM_BUILTIN, {.builtin = add}},
    {"*", SYM_BUILTIN, {.builtin = multiply}},
    {"/", SYM_BUILTIN, {.builtin = divide}},
    {"mod", SYM_BUILTIN, {.builtin = modulo}},
    {"if", SYM_SPECIAL, {.special = eval_if}},
    {"local", SYM_SPECIAL, {.special = eval_local}},
    {"lambda", SYM_SPECIAL, {.special = eval_lambda}},
    {NULL, SYM_BUILTIN, {.builtin = NULL}}
};

//-----------------------------------------------------------------------------
// 辅助函数
//-----------------------------------------------------------------------------

static Node* alloc_node(void) {
    Node* node = (Node*)calloc(1, sizeof(Node));
    return node;
}

static SymbolEntry* lookup_symbol(const char* name) {
    for (SymbolEntry* entry = builtin_symbols; entry->name; entry++) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool env_add(Env* env, const char* name, Value value) {
    if (!env || !name || env->var_count >= MAX_VARS) return false;
    
    EnvVar* var = &env->vars[env->var_count++];
    strncpy(var->name, name, MAX_IDENT - 1);
    var->value = value;
    return true;
}

static Value* env_get(Env* env, const char* name) {
    if (!env || !name) return NULL;
    
    for (size_t i = 0; i < env->var_count; i++) {
        if (strcmp(env->vars[i].name, name) == 0) {
            return &env->vars[i].value;
        }
    }
    
    return env->parent ? env_get(env->parent, name) : NULL;
}

static bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_symbol(char c) {
    return is_alpha(c) || is_digit(c) || c == '+' || c == '-' || c == '*' || c == '/' || 
           c == '=' || c == '<' || c == '>' || c == '!' || c == '?' || c == '_';
}

static void skip_whitespace(Parser* p) {
    while (*p->cur && is_whitespace(*p->cur)) {
        if (*p->cur == '\n') {
            p->line++;
            p->column = 1;
        } else {
            p->column++;
        }
        p->cur++;
    }
}

//-----------------------------------------------------------------------------
// 内置函数实现
//-----------------------------------------------------------------------------

static Value add(Node* node, Env* env) {
    Value err = {.type = VAL_ERR};
    
    if (!node || node->type != NODE_CALL || node->data.call.arg_count != 2) {
        strncpy(err.data.err, "Add requires 2 arguments", MAX_STR - 1);
        return err;
    }
    
    Value a = eval(node->data.call.args[0], env);
    if (a.type == VAL_ERR) return a;
    
    Value b = eval(node->data.call.args[1], env);
    if (b.type == VAL_ERR) return b;
    
    if (a.type != VAL_NUM || b.type != VAL_NUM) {
        strncpy(err.data.err, "Add requires numeric arguments", MAX_STR - 1);
        return err;
    }
    
    Value result = {.type = VAL_NUM};
    result.data.num = a.data.num + b.data.num;
    return result;
}

static Value multiply(Node* node, Env* env) {
    Value err = {.type = VAL_ERR};
    
    if (!node || node->type != NODE_CALL || node->data.call.arg_count != 2) {
        strncpy(err.data.err, "Multiply requires 2 arguments", MAX_STR - 1);
        return err;
    }
    
    Value a = eval(node->data.call.args[0], env);
    if (a.type == VAL_ERR) return a;
    
    Value b = eval(node->data.call.args[1], env);
    if (b.type == VAL_ERR) return b;
    
    if (a.type != VAL_NUM || b.type != VAL_NUM) {
        strncpy(err.data.err, "Multiply requires numeric arguments", MAX_STR - 1);
        return err;
    }
    
    Value result = {.type = VAL_NUM};
    result.data.num = a.data.num * b.data.num;
    return result;
}

static Value divide(Node* node, Env* env) {
    Value err = {.type = VAL_ERR};
    
    if (!node || node->type != NODE_CALL || node->data.call.arg_count != 2) {
        strncpy(err.data.err, "Divide requires 2 arguments", MAX_STR - 1);
        return err;
    }
    
    Value a = eval(node->data.call.args[0], env);
    if (a.type == VAL_ERR) return a;
    
    Value b = eval(node->data.call.args[1], env);
    if (b.type == VAL_ERR) return b;
    
    if (a.type != VAL_NUM || b.type != VAL_NUM) {
        strncpy(err.data.err, "Divide requires numeric arguments", MAX_STR - 1);
        return err;
    }
    
    if (b.data.num == 0) {
        strncpy(err.data.err, "Division by zero", MAX_STR - 1);
        return err;
    }
    
    Value result = {.type = VAL_NUM};
    result.data.num = a.data.num / b.data.num;
    return result;
}

static Value modulo(Node* node, Env* env) {
    Value err = {.type = VAL_ERR};
    
    if (!node || node->type != NODE_CALL || node->data.call.arg_count != 2) {
        strncpy(err.data.err, "Modulo requires 2 arguments", MAX_STR - 1);
        return err;
    }
    
    Value a = eval(node->data.call.args[0], env);
    if (a.type == VAL_ERR) return a;
    
    Value b = eval(node->data.call.args[1], env);
    if (b.type == VAL_ERR) return b;
    
    if (a.type != VAL_NUM || b.type != VAL_NUM) {
        strncpy(err.data.err, "Modulo requires numeric arguments", MAX_STR - 1);
        return err;
    }
    
    if (b.data.num == 0) {
        strncpy(err.data.err, "Modulo by zero", MAX_STR - 1);
        return err;
    }
    
    Value result = {.type = VAL_NUM};
    result.data.num = fmod(a.data.num, b.data.num);
    return result;
}

//-----------------------------------------------------------------------------
// 特殊形式实现
//-----------------------------------------------------------------------------

static Value eval_if(Node* node, Env* env) {
    if (!node || !node->data.if_expr.cond || !node->data.if_expr.then_expr || !node->data.if_expr.else_expr) {
        Value err = {.type = VAL_ERR};
        strncpy(err.data.err, "Invalid if expression", MAX_STR - 1);
        return err;
    }
    
    Value cond = eval(node->data.if_expr.cond, env);
    if (cond.type == VAL_ERR) return cond;
    
    if (cond.type == VAL_NUM && cond.data.num != 0) {
        return eval(node->data.if_expr.then_expr, env);
    } else {
        return eval(node->data.if_expr.else_expr, env);
    }
}

static Value eval_local(Node* node, Env* env) {
    if (!node || !node->data.local.value) {
        Value err = {.type = VAL_ERR};
        strncpy(err.data.err, "Invalid local expression", MAX_STR - 1);
        return err;
    }
    
    Value val = eval(node->data.local.value, env);
    if (val.type == VAL_ERR) return val;
    
    if (!env_add(env, node->data.local.name, val)) {
        Value err = {.type = VAL_ERR};
        strncpy(err.data.err, "Failed to add variable", MAX_STR - 1);
        return err;
    }
    
    if (node->data.local.next) {
        return eval(node->data.local.next, env);
    }
    
    return val;
}

static Value eval_lambda(Node* node, Env* env) {
    if (!node || !node->data.lambda.body) {
        Value err = {.type = VAL_ERR};
        strncpy(err.data.err, "Invalid lambda expression", MAX_STR - 1);
        return err;
    }
    
    Value val = {.type = VAL_FUN};
    val.data.fun.node = node;
    val.data.fun.env = env;
    return val;
}

//-----------------------------------------------------------------------------
// 解析器实现
//-----------------------------------------------------------------------------

static Node* parse_expr(Parser* p) {
    skip_whitespace(p);
    
    if (!*p->cur) return NULL;
    
    if (is_digit(*p->cur) || (*p->cur == '-' && is_digit(*(p->cur + 1)))) {
        return parse_number(p);
    }
    
    if (*p->cur == '(') {
        const char* start = p->cur;
        p->cur++;
        p->column++;
        
        skip_whitespace(p);
        
        // 检查特殊形式
        start = p->cur;
        while (is_symbol(*p->cur)) {
            p->column++;
            p->cur++;
        }
        
        size_t len = p->cur - start;
        p->cur = start;  // 重置位置
        p->column -= len;
        
        if (len == 2 && strncmp(start, "if", 2) == 0 && !is_symbol(*(start + 2))) {
            p->cur += 2;
            p->column += 2;
            skip_whitespace(p);
            return parse_if(p);
        }
        
        if (len == 6 && strncmp(start, "lambda", 6) == 0 && !is_symbol(*(start + 6))) {
            p->cur += 6;
            p->column += 6;
            skip_whitespace(p);
            return parse_lambda(p);
        }
        
        if (len == 5 && strncmp(start, "local", 5) == 0 && !is_symbol(*(start + 5))) {
            p->cur += 5;
            p->column += 5;
            skip_whitespace(p);
            return parse_local(p);
        }
        
        // 函数调用
        while (is_symbol(*p->cur)) {
            p->column++;
            p->cur++;
        }
        
        len = p->cur - start;
        if (len == 0) return NULL;
        
        Node* node = alloc_node();
        if (!node) return NULL;
        
        node->type = NODE_CALL;
        if (len >= MAX_IDENT) return NULL;
        
        strncpy(node->data.call.name, start, len);
        node->data.call.name[len] = '\0';
        node->data.call.arg_count = 0;
        
        skip_whitespace(p);
        
        while (*p->cur && *p->cur != ')') {
            if (node->data.call.arg_count > 0) {
                if (*p->cur != ',') return NULL;
                p->cur++;
                p->column++;
                skip_whitespace(p);
            }
            
            if (node->data.call.arg_count >= MAX_ARGS) return NULL;
            
            Node* arg = parse_expr(p);
            if (!arg) return NULL;
            
            node->data.call.args[node->data.call.arg_count++] = arg;
            
            skip_whitespace(p);
        }
        
        if (*p->cur != ')') return NULL;
        p->cur++;
        p->column++;
        
        return node;
    }
    
    if (*p->cur == '"') {
        return parse_string(p);
    }
    
    if (is_alpha(*p->cur) || *p->cur == '+' || *p->cur == '-' || *p->cur == '*' || *p->cur == '/') {
        const char* start = p->cur;
        while (is_symbol(*p->cur)) {
            p->column++;
            p->cur++;
        }
        
        size_t len = p->cur - start;
        if (len >= MAX_IDENT) return NULL;
        
        Node* node = alloc_node();
        if (!node) return NULL;
        
        node->type = NODE_SYM;
        strncpy(node->data.sym, start, len);
        node->data.sym[len] = '\0';
        
        return node;
    }
    
    return NULL;
}

static Node* parse_number(Parser* p) {
    char* end;
    double num = strtod(p->cur, &end);
    if (end == p->cur) return NULL;
    
    Node* node = alloc_node();
    if (!node) return NULL;
    
    node->type = NODE_NUM;
    node->data.num = num;
    
    p->column += (end - p->cur);
    p->cur = end;
    return node;
}

static Node* parse_symbol(Parser* p) {
    const char* start = p->cur;
    while (is_symbol(*p->cur)) {
        p->column++;
        p->cur++;
    }
    
    size_t len = p->cur - start;
    if (len >= MAX_IDENT) return NULL;
    
    Node* node = alloc_node();
    if (!node) return NULL;
    
    node->type = NODE_SYM;
    strncpy(node->data.sym, start, len);
    node->data.sym[len] = '\0';
    
    return node;
}

static Node* parse_string(Parser* p) {
    if (*p->cur != '"') return NULL;
    p->cur++;
    p->column++;
    
    const char* start = p->cur;
    while (*p->cur && *p->cur != '"') {
        if (*p->cur == '\n') {
            p->line++;
            p->column = 1;
        } else {
            p->column++;
        }
        p->cur++;
    }
    
    if (*p->cur != '"') return NULL;
    
    size_t len = p->cur - start;
    if (len >= MAX_STR) return NULL;
    
    Node* node = alloc_node();
    if (!node) return NULL;
    
    node->type = NODE_STR;
    strncpy(node->data.str, start, len);
    node->data.str[len] = '\0';
    
    p->cur++;
    p->column++;
    return node;
}

static Node* parse_call(Parser* p) {
    if (*p->cur != '(') return NULL;
    p->cur++;
    p->column++;
    
    skip_whitespace(p);
    
    const char* start = p->cur;
    while (is_symbol(*p->cur)) {
        p->column++;
        p->cur++;
    }
    
    size_t len = p->cur - start;
    if (len == 0) return NULL;
    
    Node* node = alloc_node();
    if (!node) return NULL;
    
    node->type = NODE_CALL;
    if (len >= MAX_IDENT) return NULL;
    
    strncpy(node->data.call.name, start, len);
    node->data.call.name[len] = '\0';
    node->data.call.arg_count = 0;
    
    skip_whitespace(p);
    
    while (*p->cur && *p->cur != ')') {
        if (node->data.call.arg_count > 0) {
            if (*p->cur != ',') return NULL;
            p->cur++;
            p->column++;
            skip_whitespace(p);
        }
        
        if (node->data.call.arg_count >= MAX_ARGS) return NULL;
        
        Node* arg = parse_expr(p);
        if (!arg) return NULL;
        
        node->data.call.args[node->data.call.arg_count++] = arg;
        
        skip_whitespace(p);
    }
    
    if (*p->cur != ')') return NULL;
    p->cur++;
    p->column++;
    
    return node;
}

static Node* parse_if(Parser* p) {
    Node* node = alloc_node();
    if (!node) return NULL;
    
    node->type = NODE_IF;
    
    // Parse condition
    node->data.if_expr.cond = parse_expr(p);
    if (!node->data.if_expr.cond) return NULL;
    
    skip_whitespace(p);
    
    if (*p->cur != ',') return NULL;
    p->cur++;
    p->column++;
    
    skip_whitespace(p);
    
    // Parse then expression
    node->data.if_expr.then_expr = parse_expr(p);
    if (!node->data.if_expr.then_expr) return NULL;
    
    skip_whitespace(p);
    
    if (*p->cur != ',') return NULL;
    p->cur++;
    p->column++;
    
    skip_whitespace(p);
    
    // Parse else expression
    node->data.if_expr.else_expr = parse_expr(p);
    if (!node->data.if_expr.else_expr) return NULL;
    
    skip_whitespace(p);
    
    if (*p->cur != ')') return NULL;
    p->cur++;
    p->column++;
    
    return node;
}

static Node* parse_lambda(Parser* p) {
    Node* node = alloc_node();
    if (!node) return NULL;
    
    node->type = NODE_LAMBDA;
    
    if (*p->cur != '(') return NULL;
    p->cur++;
    p->column++;
    
    skip_whitespace(p);
    
    // Parse parameter
    const char* start = p->cur;
    while (is_symbol(*p->cur)) {
        p->column++;
        p->cur++;
    }
    
    size_t len = p->cur - start;
    if (len == 0 || len >= MAX_IDENT) return NULL;
    
    strncpy(node->data.lambda.param, start, len);
    node->data.lambda.param[len] = '\0';
    
    skip_whitespace(p);
    
    if (*p->cur != ')') return NULL;
    p->cur++;
    p->column++;
    
    skip_whitespace(p);
    
    if (*p->cur != ',') return NULL;
    p->cur++;
    p->column++;
    
    skip_whitespace(p);
    
    // Parse body
    node->data.lambda.body = parse_expr(p);
    if (!node->data.lambda.body) return NULL;
    
    skip_whitespace(p);
    
    if (*p->cur != ')') return NULL;
    p->cur++;
    p->column++;
    
    return node;
}

static Node* parse_local(Parser* p) {
    Node* node = alloc_node();
    if (!node) return NULL;
    
    node->type = NODE_LOCAL;
    
    if (*p->cur != '(') return NULL;
    p->cur++;
    p->column++;
    
    skip_whitespace(p);
    
    // Parse variable name
    const char* start = p->cur;
    while (is_symbol(*p->cur)) {
        p->column++;
        p->cur++;
    }
    
    size_t len = p->cur - start;
    if (len == 0 || len >= MAX_IDENT) return NULL;
    
    strncpy(node->data.local.name, start, len);
    node->data.local.name[len] = '\0';
    
    skip_whitespace(p);
    
    if (*p->cur != ',') return NULL;
    p->cur++;
    p->column++;
    
    skip_whitespace(p);
    
    // Parse value
    node->data.local.value = parse_expr(p);
    if (!node->data.local.value) return NULL;
    
    skip_whitespace(p);
    
    if (*p->cur == ',') {
        p->cur++;
        p->column++;
        
        skip_whitespace(p);
        
        // Parse next local binding
        node->data.local.next = parse_expr(p);
        if (!node->data.local.next) return NULL;
    } else {
        node->data.local.next = NULL;
    }
    
    skip_whitespace(p);
    
    if (*p->cur != ')') return NULL;
    p->cur++;
    p->column++;
    
    skip_whitespace(p);
    
    if (*p->cur != ')') return NULL;
    p->cur++;
    p->column++;
    
    return node;
}

//-----------------------------------------------------------------------------
// 求值器实现
//-----------------------------------------------------------------------------

static Value eval(Node* node, Env* env) {
    if (!node) {
        Value err = {.type = VAL_ERR};
        strncpy(err.data.err, "Cannot evaluate NULL node", MAX_STR - 1);
        return err;
    }
    
    switch (node->type) {
        case NODE_NUM: {
            Value val = {.type = VAL_NUM};
            val.data.num = node->data.num;
            return val;
        }
        
        case NODE_SYM: {
            // 先查找变量
            Value* val = env_get(env, node->data.sym);
            if (val) return *val;
            
            // 再查找内置函数和特殊形式
            SymbolEntry* entry = lookup_symbol(node->data.sym);
            if (entry) {
                Value err = {.type = VAL_ERR};
                strncpy(err.data.err, "Function must be called", MAX_STR - 1);
                return err;
            }
            
            Value err = {.type = VAL_ERR};
            strncpy(err.data.err, "Undefined variable", MAX_STR - 1);
            return err;
        }
        
        case NODE_STR: {
            Value val = {.type = VAL_STR};
            strncpy(val.data.str, node->data.str, MAX_STR - 1);
            return val;
        }
        
        case NODE_BOOL: {
            Value val = {.type = VAL_BOOL};
            val.data.boolean = node->data.boolean;
            return val;
        }
        
        case NODE_LIST: {
            Value val = {.type = VAL_LIST};
            val.data.list = node->data.list;
            return val;
        }
        
        case NODE_IF:
            return eval_if(node, env);
            
        case NODE_CALL: {
            // 先查找内置函数和特殊形式
            SymbolEntry* entry = lookup_symbol(node->data.call.name);
            if (entry) {
                if (entry->type == SYM_BUILTIN) {
                    return entry->func.builtin(node, env);
                } else {
                    return entry->func.special(node, env);
                }
            }
            
            // 再查找用户定义的函数
            Value* fun_val = env_get(env, node->data.call.name);
            if (!fun_val || fun_val->type != VAL_FUN) {
                Value err = {.type = VAL_ERR};
                strncpy(err.data.err, "Undefined function", MAX_STR - 1);
                return err;
            }
            
            Node* lambda = fun_val->data.fun.node;
            if (!lambda || lambda->type != NODE_LAMBDA) {
                Value err = {.type = VAL_ERR};
                strncpy(err.data.err, "Invalid function value", MAX_STR - 1);
                return err;
            }
            
            if (node->data.call.arg_count != 1) {
                Value err = {.type = VAL_ERR};
                strncpy(err.data.err, "Wrong number of arguments", MAX_STR - 1);
                return err;
            }
            
            Value arg = eval(node->data.call.args[0], env);
            if (arg.type == VAL_ERR) return arg;
            
            Env new_env = {0};
            new_env.parent = fun_val->data.fun.env;
            new_env.depth = env->depth + 1;
            
            if (new_env.depth > MAX_DEPTH) {
                Value err = {.type = VAL_ERR};
                strncpy(err.data.err, "Maximum recursion depth exceeded", MAX_STR - 1);
                return err;
            }
            
            if (!env_add(&new_env, lambda->data.lambda.param, arg)) {
                Value err = {.type = VAL_ERR};
                strncpy(err.data.err, "Failed to bind parameter", MAX_STR - 1);
                return err;
            }
            
            return eval(lambda->data.lambda.body, &new_env);
        }
        
        case NODE_LOCAL:
            return eval_local(node, env);
            
        case NODE_LAMBDA:
            return eval_lambda(node, env);
            
        default: {
            Value err = {.type = VAL_ERR};
            strncpy(err.data.err, "Unknown node type", MAX_STR - 1);
            return err;
        }
    }
}

//-----------------------------------------------------------------------------
// 主程序
//-----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <expression>\n", argv[0]);
        return 1;
    }
    
    Parser parser = {
        .cur = argv[1],
        .line = 1,
        .column = 1
    };
    
    Node* node = parse_expr(&parser);
    if (!node) {
        fprintf(stderr, "Parse error at line %zu, column %zu\n", parser.line, parser.column);
        return 1;
    }
    
    skip_whitespace(&parser);
    if (*parser.cur) {
        fprintf(stderr, "Unexpected characters after expression at line %zu, column %zu\n", 
                parser.line, parser.column);
        return 1;
    }
    
    Env env = {0};
    Value result = eval(node, &env);
    
    switch (result.type) {
        case VAL_NUM:
            printf("%g\n", result.data.num);
            break;
        case VAL_STR:
            printf("\"%s\"\n", result.data.str);
            break;
        case VAL_BOOL:
            printf("%s\n", result.data.boolean ? "true" : "false");
            break;
        case VAL_LIST:
            printf("<list>\n");
            break;
        case VAL_FUN:
            printf("<lambda>\n");
            break;
        case VAL_ERR:
            fprintf(stderr, "Error: %s\n", result.data.err);
            return 1;
    }
    
    return 0;
} 