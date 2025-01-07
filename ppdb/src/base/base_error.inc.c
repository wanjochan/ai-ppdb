/*
 * base_error.inc.c - Error handling implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Thread-local error context
static __thread ppdb_error_context_t g_error_context;

// Error statistics
typedef struct ppdb_error_stats_s {
    uint64_t total_errors;        // Total number of errors
    uint64_t error_by_code[256];  // Errors by error code
} ppdb_error_stats_t;

static ppdb_error_stats_t g_error_stats;
static ppdb_base_mutex_t* g_error_mutex = NULL;

void ppdb_error_init(void) {
    if (g_error_mutex) return;
    ppdb_base_mutex_create(&g_error_mutex);
    memset(&g_error_stats, 0, sizeof(g_error_stats));
}

void ppdb_error_cleanup(void) {
    if (g_error_mutex) {
        ppdb_base_mutex_destroy(g_error_mutex);
        g_error_mutex = NULL;
    }
}

void ppdb_error_set_context(ppdb_error_context_t* ctx) {
    if (!ctx) return;

    memcpy(&g_error_context, ctx, sizeof(ppdb_error_context_t));

    // Update statistics
    if (g_error_mutex) {
        ppdb_base_mutex_lock(g_error_mutex);
        g_error_stats.total_errors++;
        if (ctx->code < 256) {
            g_error_stats.error_by_code[ctx->code]++;
        }
        ppdb_base_mutex_unlock(g_error_mutex);
    }
}

const ppdb_error_context_t* ppdb_error_get_context(void) {
    return &g_error_context;
}

void ppdb_error_clear_context(void) {
    memset(&g_error_context, 0, sizeof(ppdb_error_context_t));
}

const char* ppdb_error_to_string(ppdb_error_t err) {
    switch (err) {
        case PPDB_OK:
            return "Success";
        case PPDB_BASE_ERR_MUTEX:
            return "Mutex error";
        case PPDB_BASE_ERR_RWLOCK:
            return "Read-write lock error";
        case PPDB_BASE_ERR_THREAD:
            return "Thread error";
        case PPDB_BASE_ERR_SYNC:
            return "Synchronization error";
        case PPDB_BASE_ERR_POOL:
            return "Memory pool error";
        case PPDB_BASE_ERR_MEMORY:
            return "Memory allocation error";
        case PPDB_BASE_ERR_IO:
            return "IO error";
        case PPDB_BASE_ERR_PARAM:
            return "Invalid parameter";
        case PPDB_BASE_ERR_INVALID_STATE:
            return "Invalid state";
        case PPDB_BASE_ERR_CONFIG:
            return "Configuration error";
        default:
            return "Unknown error";
    }
}

void ppdb_error_set(ppdb_error_t code, const char* file,
                   int line, const char* func,
                   const char* fmt, ...) {
    ppdb_error_context_t ctx;
    va_list args;

    ctx.code = code;
    ctx.file = file;
    ctx.line = line;
    ctx.func = func;

    va_start(args, fmt);
    vsnprintf(ctx.message, sizeof(ctx.message), fmt, args);
    va_end(args);

    ppdb_error_set_context(&ctx);
}

void ppdb_error_get_stats(ppdb_error_stats_t* stats) {
    if (!stats || !g_error_mutex) return;

    ppdb_base_mutex_lock(g_error_mutex);
    memcpy(stats, &g_error_stats, sizeof(ppdb_error_stats_t));
    ppdb_base_mutex_unlock(g_error_mutex);
}

void ppdb_error_reset_stats(void) {
    if (!g_error_mutex) return;

    ppdb_base_mutex_lock(g_error_mutex);
    memset(&g_error_stats, 0, sizeof(ppdb_error_stats_t));
    ppdb_base_mutex_unlock(g_error_mutex);
}