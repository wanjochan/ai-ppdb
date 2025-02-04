#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "internal/infra/InfraLog.h"

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

static void log_message(InfraLog* self, LogLevel level, const char* format, va_list args) {
    if (!self || level < self->min_level) return;
    
    char time_str[32];
    get_time_str(time_str, sizeof(time_str));
    
    fprintf(stderr, "[%s] [%s] ", time_str, level_to_str(level));
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

static void infra_log_init(InfraLog* self) {
    if (!self) return;
    
    // Initialize properties
    self->min_level = LOG_LEVEL_INFO;
    
    // Initialize methods
    self->new = infra_log_new;
    self->free = infra_log_free;
    self->set_level = infra_log_set_level;
    self->debug = infra_log_debug;
    self->info = infra_log_info;
    self->warn = infra_log_warn;
    self->error = infra_log_error;
}

// Public functions
InfraLog* infra_log_new(void) {
    InfraLog* log = (InfraLog*)malloc(sizeof(InfraLog));
    if (log) {
        infra_log_init(log);
    }
    return log;
}

void infra_log_free(InfraLog* self) {
    if (!self) return;
    free(self);
}

void infra_log_set_level(InfraLog* self, LogLevel level) {
    if (!self) return;
    self->min_level = level;
}

void infra_log_debug(InfraLog* self, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void infra_log_info(InfraLog* self, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void infra_log_warn(InfraLog* self, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_WARN, format, args);
    va_end(args);
}

void infra_log_error(InfraLog* self, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_ERROR, format, args);
    va_end(args);
}
