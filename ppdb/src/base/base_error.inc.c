/*
 * base_error.inc.c - Error handling implementation
 */

#include <cosmopolitan.h>
#include "../internal/base.h"

// Global error context
static ppdb_error_context_t g_error_context;

// Error handling functions
void ppdb_error_init(void) {
    memset(&g_error_context, 0, sizeof(g_error_context));
    g_error_context.code = PPDB_OK;
}

void ppdb_error_set_context(ppdb_error_context_t* ctx) {
    if (!ctx) return;
    memcpy(&g_error_context, ctx, sizeof(ppdb_error_context_t));
}

const ppdb_error_context_t* ppdb_error_get_context(void) {
    return &g_error_context;
}

const char* ppdb_error_to_string(ppdb_error_t err) {
    switch (err) {
        case PPDB_OK:
            return "Success";
        case PPDB_ERR_MEMORY:
            return "Memory allocation failed";
        case PPDB_ERR_PARAM:
            return "Invalid parameter";
        case PPDB_ERR_EXISTS:
            return "Object already exists";
        case PPDB_ERR_NOT_FOUND:
            return "Object not found";
        case PPDB_ERR_INVALID_STATE:
            return "Invalid state";
        case PPDB_ERR_BUSY:
            return "Resource busy";
        case PPDB_ERR_BUFFER_TOO_SMALL:
            return "Buffer too small";
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
            return "Memory error";
        case PPDB_BASE_ERR_IO:
            return "I/O error";
        default:
            return "Unknown error";
    }
} 