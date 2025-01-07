#include "ppdb/ast_runtime.h"

ast_env_t* ast_env_new(ast_env_t* parent) {
    ast_env_t* env = malloc(sizeof(ast_env_t));
    env->entries = NULL;
    env->parent = parent;
    return env;
}

void ast_env_free(ast_env_t* env) {
    if (!env) return;
    
    env_entry_t* current = env->entries;
    while (current) {
        env_entry_t* next = current->next;
        free(current->name);
        ast_node_free(current->value);
        free(current);
        current = next;
    }
    free(env);
}

ast_node_t* ast_env_lookup(ast_env_t* env, const char* name) {
    if (!env) return NULL;
    
    env_entry_t* current = env->entries;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current->value;
        }
        current = current->next;
    }
    
    return env->parent ? ast_env_lookup(env->parent, name) : NULL;
}

void ast_env_define(ast_env_t* env, const char* name, ast_node_t* value) {
    if (!env) return;
    
    env_entry_t* current = env->entries;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            ast_node_free(current->value);
            current->value = value;
            return;
        }
        current = current->next;
    }
    
    env_entry_t* entry = malloc(sizeof(env_entry_t));
    entry->name = strdup(name);
    entry->value = value;
    entry->next = env->entries;
    env->entries = entry;
} 