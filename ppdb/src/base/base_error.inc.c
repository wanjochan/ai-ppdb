/*
 * base_error.inc.c - Error handling implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

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
            return "Memory error";
        case PPDB_ERR_PARAM:
            return "Invalid parameter";
        case PPDB_ERR_EXISTS:
            return "Already exists";
        case PPDB_ERR_NOT_FOUND:
            return "Not found";
        case PPDB_ERR_BUSY:
            return "Resource busy";
        case PPDB_ERR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        case PPDB_BASE_ERR_MUTEX:
            return "Base mutex error";
        case PPDB_BASE_ERR_RWLOCK:
            return "Base read-write lock error";
        case PPDB_BASE_ERR_THREAD:
            return "Base thread error";
        case PPDB_BASE_ERR_SYNC:
            return "Base synchronization error";
        case PPDB_BASE_ERR_POOL:
            return "Base memory pool error";
        case PPDB_BASE_ERR_MEMORY:
            return "Base memory error";
        case PPDB_BASE_ERR_IO:
            return "Base I/O error";
        case PPDB_BASE_ERR_PARAM:
            return "Base invalid parameter";
        case PPDB_BASE_ERR_INVALID_STATE:
            return "Base invalid state";
        case PPDB_BASE_ERR_CONFIG:
            return "Base configuration error";
        default:
            return "Unknown error";
    }
} 