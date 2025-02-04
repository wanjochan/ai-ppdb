#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "internal/infrax/InfraxCore.h"

// Private functions
static void infrax_core_print(InfraxCore *self) {
    if (!self) return;
    printf("InfraxCore: data=%d\n", self->data);
}

static void infrax_core_init(InfraxCore *self) {
    if (!self) return;
    
    // Initialize data
    self->data = 0;
    self->min_log_level = LOG_LEVEL_INFO;  // Default log level
    
    // Initialize methods
    self->new = infrax_core_new;
    self->free = infrax_core_free;
    self->print = infrax_core_print;
    self->set_log_level = infrax_core_set_log_level;
    self->log_message = infrax_core_log_message;
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

void infrax_core_set_log_level(InfraxCore *self, LogLevel level) {
    if (!self) return;
    self->min_log_level = level;
}

void infrax_core_log_message(InfraxCore *self, LogLevel level, const char* format, ...) {
    if (!self || level < self->min_log_level) return;

    const char* level_str = "";
    switch (level) {
        case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case LOG_LEVEL_INFO:  level_str = "INFO"; break;
        case LOG_LEVEL_WARN:  level_str = "WARN"; break;
        case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
    }

    printf("[%s] ", level_str);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
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
