#ifndef PPDB_AST_H
#define PPDB_AST_H

#include "cosmopolitan.h"

// 类型定义
typedef enum {
    ATOM,
    CONS
} sexp_type_t;

typedef enum {
    NUMBER,
    SYMBOL
} atom_type_t;

typedef struct sexp {
    sexp_type_t type;
    union {
        struct {
            atom_type_t type;
            union {
                double number;
                char* symbol;
            } value;
        } atom;
        struct {
            struct sexp* car;
            struct sexp* cdr;
        } cons;
    } value;
} sexp_t;

typedef struct env_entry {
    char* name;
    sexp_t* value;
    struct env_entry* next;
} env_entry_t;

typedef struct env {
    env_entry_t* entries;
    struct env* parent;
} env_t;

// 构造函数
sexp_t* make_number(double value);
sexp_t* make_symbol(const char* name);
sexp_t* make_cons(sexp_t* car, sexp_t* cdr);
sexp_t* make_nil(void);

// 谓词
int is_nil(sexp_t* expr);
int is_atom(sexp_t* expr);
int is_cons(sexp_t* expr);
int is_number(sexp_t* expr);
int is_symbol(sexp_t* expr);

// 访问器
sexp_t* car(sexp_t* expr);
sexp_t* cdr(sexp_t* expr);
double number_value(sexp_t* expr);
const char* symbol_value(sexp_t* expr);

// 环境操作
env_t* env_new(env_t* parent);
void env_free(env_t* env);
sexp_t* env_lookup(env_t* env, const char* name);
void env_define(env_t* env, const char* name, sexp_t* value);

// 解析和求值
sexp_t* parse(const char* input);
sexp_t* eval(sexp_t* expr, env_t* env);

// 内存管理
void sexp_free(sexp_t* expr);

#endif // PPDB_AST_H 