#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "internal/infrax/InfraxCore.h"

// Forward declarations of internal functions
static void infrax_core_init(InfraxCore* self);
// static void infrax_core_print(InfraxCore* self);

// Printf forwarding implementation
int infrax_core_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}

// Parameter forwarding function implementation
void* infrax_core_forward_call(void* (*target_func)(va_list), ...) {
    va_list args;
    va_start(args, target_func);
    void* result = target_func(args);
    va_end(args);
    return result;
}

// // Private functions
// static void infrax_core_print(InfraxCore *self) {
//     if (!self) return;
//     printf("InfraxCore: data=%d\n", self->data);
// }

static void infrax_core_init(InfraxCore *self) {
    if (!self) return;
    
    // Initialize data
    self->data = 0;
    
    // Initialize methods
    self->new = infrax_core_new;
    self->free = infrax_core_free;
    // self->print = infrax_core_print;
    self->forward_call = infrax_core_forward_call;
    self->printf = infrax_core_printf;
}

// Public functions
InfraxCore* infrax_core_new(void) {
    InfraxCore *core = (InfraxCore*)malloc(sizeof(InfraxCore));
    if (core) {
        infrax_core_init(core);
    }
    return core;
}

void infrax_core_free(InfraxCore *self) {
    if (!self) return;
    free(self);
}

// 定义全局的 InfraxCore 实例，确保只初始化一次
static InfraxCore global_infra_core = {0};
static int global_infra_core_initialized = 0;

// 提供 getter 函数，用于获取全局实例
InfraxCore* get_global_infra_core(void) {
    if (!global_infra_core_initialized) {
        infrax_core_init(&global_infra_core);
        global_infra_core_initialized = 1;
    }
    return &global_infra_core;
}
