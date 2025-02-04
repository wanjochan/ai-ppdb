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
    if (!self || level < self->min_level) return;
    
    char time_str[32];
    get_time_str(time_str, sizeof(time_str));
    
    fprintf(stderr, "[%s] [%s] ", time_str, level_to_str(level));
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

static void infrax_log_init(InfraxLog* self) {
    if (!self) return;
    
    // Initialize properties
    self->min_level = LOG_LEVEL_INFO;
    
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
    InfraxLog* log = (InfraxLog*)malloc(sizeof(InfraxLog));
    if (log) {
        infrax_log_init(log);
    }
    return log;
}

void infrax_log_free(InfraxLog* self) {
    if (!self) return;
    free(self);
}

void infrax_log_set_level(InfraxLog* self, LogLevel level) {
    if (!self) return;
    self->min_level = level;
}

void infrax_log_debug(InfraxLog* self, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void infrax_log_info(InfraxLog* self, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void infrax_log_warn(InfraxLog* self, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_WARN, format, args);
    va_end(args);
}

void infrax_log_error(InfraxLog* self, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_ERROR, format, args);
    va_end(args);
}
