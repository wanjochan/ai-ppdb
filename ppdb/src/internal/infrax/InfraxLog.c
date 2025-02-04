#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "internal/infrax/InfraxLog.h"

// Private functions
static void get_time_str(char* buffer, size_t size) {
    time_t now;
    struct tm* timeinfo;
    
    time(&now);
    timeinfo = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", timeinfo);
}

static const char* level_to_str(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

static void log_message(InfraxLog* self, LogLevel level, const char* format, va_list args) {
    if (!self || !self->core || !format) return;
    
    char time_str[32];
    get_time_str(time_str, sizeof(time_str));
    
    // First format the message with the timestamp
    char msg_buffer[1024];  // Reasonable buffer size
    int prefix_len = snprintf(msg_buffer, sizeof(msg_buffer), "[%s] ", time_str);
    if (prefix_len < 0 || prefix_len >= sizeof(msg_buffer)) return;
    
    // Then format the actual message
    va_list args_copy;
    va_copy(args_copy, args);
    vsnprintf(msg_buffer + prefix_len, sizeof(msg_buffer) - prefix_len, format, args_copy);
    va_end(args_copy);
    
    // Now pass the formatted message to core
    if (self->core->log_message) {
        self->core->log_message(self->core, level, "%s", msg_buffer);
    }
}

static void infrax_log_init(InfraxLog *self) {
    if (!self) return;
    
    // Get the global InfraxCore instance
    self->core = get_global_infra_core();
    
    // Initialize methods
    self->new = infrax_log_new;
    self->free = infrax_log_free;
    self->set_level = infrax_log_set_level;
    self->debug = infrax_log_debug;
    self->info = infrax_log_info;
    self->warn = infrax_log_warn;
    self->error = infrax_log_error;
}

// Public functions
InfraxLog* infrax_log_new(void) {
    InfraxLog *log = (InfraxLog*)malloc(sizeof(InfraxLog));
    if (log) {
        infrax_log_init(log);
    }
    return log;
}

void infrax_log_free(InfraxLog *self) {
    if (!self) return;
    // Note: We don't free core as it's a reference to the global instance
    free(self);
}

void infrax_log_set_level(InfraxLog *self, LogLevel level) {
    if (!self || !self->core) return;
    self->core->set_log_level(self->core, level);
}

void infrax_log_debug(InfraxLog *self, const char* format, ...) {
    if (!self || !self->core) return;
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void infrax_log_info(InfraxLog *self, const char* format, ...) {
    if (!self || !self->core) return;
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void infrax_log_warn(InfraxLog *self, const char* format, ...) {
    if (!self || !self->core) return;
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_WARN, format, args);
    va_end(args);
}

void infrax_log_error(InfraxLog *self, const char* format, ...) {
    if (!self || !self->core) return;
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

// Global instance
static InfraxLog global_infra_log = {0};
static int global_infra_log_initialized = 0;

InfraxLog* get_global_infra_log(void) {
    if (!global_infra_log_initialized) {
        infrax_log_init(&global_infra_log);
        global_infra_log_initialized = 1;
    }
    return &global_infra_log;
}