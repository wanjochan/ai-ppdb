#include "ppdb/ast_runtime.h"
#include "cosmopolitan.h"

/* Runtime environment entry */
typedef struct env_entry {
    char *name;
    ast_node_t *value;
    struct env_entry *next;
} env_entry_t;

/* Global runtime environment */
static env_entry_t *global_env = NULL;

/* Runtime environment functions */
ast_node_t *env_lookup(const char *name) {
    for (env_entry_t *entry = global_env; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) return entry->value;
    }
    return NULL;
}

void env_define(const char *name, ast_node_t *value) {
    env_entry_t *entry = malloc(sizeof(env_entry_t));
    if (!entry) return;
    
    entry->name = strdup(name);
    if (!entry->name) {
        free(entry);
        return;
    }
    
    entry->value = value;
    entry->next = global_env;
    global_env = entry;
}

/* Runtime AST node creation */
ast_node_t *ast_create_number(double value) {
    ast_node_t *node = malloc(sizeof(ast_node_t));
    if (!node) return NULL;
    node->type = AST_NUMBER;
    node->value.number_value = value;
    return node;
}

ast_node_t *ast_create_symbol(const char *name) {
    ast_node_t *node = malloc(sizeof(ast_node_t));
    if (!node) return NULL;
    node->type = AST_SYMBOL;
    if (!(node->value.symbol.name = strdup(name))) {
        free(node);
        return NULL;
    }
    return node;
}

ast_node_t *ast_create_call(ast_node_t *func, ast_node_t **args, size_t arg_count) {
    ast_node_t *node = malloc(sizeof(ast_node_t));
    if (!node) return NULL;
    
    node->type = AST_CALL;
    node->value.call.func = func;
    
    if (arg_count > 0) {
        node->value.call.args = malloc(sizeof(ast_node_t *) * arg_count);
        if (!node->value.call.args) {
            free(node);
            return NULL;
        }
        memcpy(node->value.call.args, args, sizeof(ast_node_t *) * arg_count);
    } else {
        node->value.call.args = NULL;
    }
    
    node->value.call.arg_count = arg_count;
    return node;
}

void ast_free(ast_node_t *node) {
    if (!node) return;
    switch (node->type) {
        case AST_SYMBOL:
            free(node->value.symbol.name);
            break;
        case AST_CALL:
            ast_free(node->value.call.func);
            for (size_t i = 0; i < node->value.call.arg_count; i++) {
                ast_free(node->value.call.args[i]);
            }
            free(node->value.call.args);
            break;
    }
    free(node);
}

ast_node_t *ast_clone(ast_node_t *node) {
    if (!node) return NULL;
    
    switch (node->type) {
        case AST_NUMBER:
            return ast_create_number(node->value.number_value);
            
        case AST_SYMBOL:
            return ast_create_symbol(node->value.symbol.name);
            
        case AST_CALL: {
            ast_node_t *func_clone = ast_clone(node->value.call.func);
            if (!func_clone) return NULL;
            
            ast_node_t **args_clone = NULL;
            if (node->value.call.arg_count > 0) {
                args_clone = malloc(sizeof(ast_node_t *) * node->value.call.arg_count);
                if (!args_clone) {
                    ast_free(func_clone);
                    return NULL;
                }
                
                for (size_t i = 0; i < node->value.call.arg_count; i++) {
                    args_clone[i] = ast_clone(node->value.call.args[i]);
                    if (!args_clone[i]) {
                        for (size_t j = 0; j < i; j++) ast_free(args_clone[j]);
                        free(args_clone);
                        ast_free(func_clone);
                        return NULL;
                    }
                }
            }
            
            ast_node_t *result = ast_create_call(func_clone, args_clone, node->value.call.arg_count);
            if (!result) {
                for (size_t i = 0; i < node->value.call.arg_count; i++) ast_free(args_clone[i]);
                free(args_clone);
                ast_free(func_clone);
            }
            return result;
        }
    }
    return NULL;
}

ast_node_t *ast_eval(ast_node_t *node) {
    if (!node) return NULL;
    
    switch (node->type) {
        case AST_NUMBER:
            return ast_clone(node);
            
        case AST_SYMBOL: {
            ast_node_t *value = env_lookup(node->value.symbol.name);
            return value ? ast_clone(value) : NULL;
        }
            
        case AST_CALL: {
            if (!node->value.call.func || node->value.call.func->type != AST_SYMBOL) 
                return NULL;
                
            // 查找函数
            const char *func_name = node->value.call.func->value.symbol.name;
            
            // 处理内置函数
            if (strcmp(func_name, "local") == 0) {
                if (node->value.call.arg_count != 2 || 
                    node->value.call.args[0]->type != AST_SYMBOL) 
                    return NULL;
                
                ast_node_t *value = ast_eval(node->value.call.args[1]);
                if (!value) return NULL;
                
                env_define(node->value.call.args[0]->value.symbol.name, value);
                return ast_clone(value);
            }
            
            if (strcmp(func_name, "if") == 0) {
                if (node->value.call.arg_count != 3) return NULL;
                
                ast_node_t *cond = ast_eval(node->value.call.args[0]);
                if (!cond || cond->type != AST_NUMBER) {
                    ast_free(cond);
                    return NULL;
                }
                
                size_t branch = cond->value.number_value != 0 ? 1 : 2;
                ast_free(cond);
                
                return ast_eval(node->value.call.args[branch]);
            }
            
            if (strcmp(func_name, "while") == 0) {
                if (node->value.call.arg_count != 2) return NULL;
                
                ast_node_t *last_result = NULL;
                while (1) {
                    ast_node_t *cond = ast_eval(node->value.call.args[0]);
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
                    
                    last_result = ast_eval(node->value.call.args[1]);
                    if (!last_result) return NULL;
                }
            }
            
            // 未知函数
            return NULL;
        }
    }
    
    return NULL;
} 