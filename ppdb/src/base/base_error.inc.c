/*
 * base_error.inc.c - Error Handling Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Global error context protected by mutex
static ppdb_error_context_t error_context = {
    .code = PPDB_OK,
    .file = NULL,
    .line = 0,
    .func = NULL,
    .message = {0}
};

static ppdb_base_mutex_t error_mutex;

ppdb_error_t ppdb_error_init(void) {
    ppdb_base_mutex_create(&error_mutex);
    ppdb_base_mutex_lock(&error_mutex);
    error_context.code = PPDB_OK;
    error_context.file = NULL;
    error_context.line = 0;
    error_context.func = NULL;
    error_context.message[0] = '\0';
    ppdb_base_mutex_unlock(&error_mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_error_set_context(const ppdb_error_context_t* ctx) {
    if (!ctx) return PPDB_BASE_ERR_PARAM;
    ppdb_base_mutex_lock(&error_mutex);
    memcpy(&error_context, ctx, sizeof(ppdb_error_context_t));
    ppdb_base_mutex_unlock(&error_mutex);
    return PPDB_OK;
}

const ppdb_error_context_t* ppdb_error_get_context(void) {
    // Note: This is not thread-safe if the caller keeps the pointer
    // They should copy the data instead of keeping the pointer
    return &error_context;
}

void ppdb_error_clear_context(void) {
    ppdb_base_mutex_lock(&error_mutex);
    error_context.code = PPDB_OK;
    error_context.file = NULL;
    error_context.line = 0;
    error_context.func = NULL;
    error_context.message[0] = '\0';
    ppdb_base_mutex_unlock(&error_mutex);
}

ppdb_error_t ppdb_error_set(ppdb_error_t code, const char* file, int line, const char* func, const char* fmt, ...) {
    if (!file || !func || !fmt) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_mutex_lock(&error_mutex);
    error_context.code = code;
    error_context.file = file;
    error_context.line = line;
    error_context.func = func;

    va_list args;
    va_start(args, fmt);
    vsnprintf(error_context.message, PPDB_MAX_ERROR_MESSAGE, fmt, args);
    va_end(args);
    ppdb_base_mutex_unlock(&error_mutex);

    return code;
}

ppdb_error_t ppdb_error_get_code(void) {
    ppdb_base_mutex_lock(&error_mutex);
    ppdb_error_t code = error_context.code;
    ppdb_base_mutex_unlock(&error_mutex);
    return code;
}

const char* ppdb_error_get_message(void) {
    // Note: This is not thread-safe if the caller keeps the pointer
    // They should copy the string instead of keeping the pointer
    return error_context.message;
}

const char* ppdb_error_get_file(void) {
    // Note: This is not thread-safe if the caller keeps the pointer
    // They should copy the string instead of keeping the pointer
    return error_context.file;
}

int ppdb_error_get_line(void) {
    ppdb_base_mutex_lock(&error_mutex);
    int line = error_context.line;
    ppdb_base_mutex_unlock(&error_mutex);
    return line;
}

const char* ppdb_error_get_func(void) {
    // Note: This is not thread-safe if the caller keeps the pointer
    // They should copy the string instead of keeping the pointer
    return error_context.func;
}

void ppdb_error_format_message(char* buffer, size_t size) {
    if (!buffer || size == 0) return;

    ppdb_base_mutex_lock(&error_mutex);
    snprintf(buffer, size, "Error %d at %s:%d in %s: %s",
             error_context.code,
             error_context.file ? error_context.file : "unknown",
             error_context.line,
             error_context.func ? error_context.func : "unknown",
             error_context.message);
    ppdb_base_mutex_unlock(&error_mutex);
}

bool ppdb_error_is_error(ppdb_error_t code) {
    return code != PPDB_OK;
}

void ppdb_error_log(const char* fmt, ...) {
    va_list args;
    char buffer[PPDB_MAX_ERROR_MESSAGE];

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    fprintf(stderr, "[ERROR] %s\n", buffer);
}

void ppdb_error_debug(const char* fmt, ...) {
#ifdef PPDB_DEBUG
    va_list args;
    char buffer[PPDB_MAX_ERROR_MESSAGE];

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    fprintf(stderr, "[DEBUG] %s\n", buffer);
#else
    (void)fmt;
#endif
}

void ppdb_error_cleanup(void) {
    ppdb_base_mutex_lock(&error_mutex);
    error_context.code = PPDB_OK;
    error_context.file = NULL;
    error_context.line = 0;
    error_context.func = NULL;
    error_context.message[0] = '\0';
    ppdb_base_mutex_unlock(&error_mutex);
    ppdb_base_mutex_destroy(&error_mutex);
}

const char* ppdb_error_to_string(ppdb_error_t code) {
    switch (code) {
        case PPDB_OK:
            return "Success";
        case PPDB_BASE_ERR_PARAM:
            return "Invalid parameter";
        case PPDB_BASE_ERR_MEMORY:
            return "Memory error";
        case PPDB_BASE_ERR_SYSTEM:
            return "System error";
        case PPDB_BASE_ERR_NOT_FOUND:
            return "Not found";
        case PPDB_BASE_ERR_EXISTS:
            return "Already exists";
        case PPDB_BASE_ERR_TIMEOUT:
            return "Timeout";
        case PPDB_BASE_ERR_BUSY:
            return "Resource busy";
        case PPDB_BASE_ERR_FULL:
            return "Resource full";
        case PPDB_BASE_ERR_EMPTY:
            return "Resource empty";
        case PPDB_BASE_ERR_IO:
            return "I/O error";
        default:
            return "Unknown error";
    }
} 