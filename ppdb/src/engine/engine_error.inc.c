/*
 * engine_error.inc.c - Engine layer error handling implementation
 */

#include <cosmopolitan.h>
#include "internal/engine.h"

// Error message conversion function
const char* ppdb_engine_strerror(ppdb_error_t err) {
    switch (err) {
        case PPDB_ENGINE_ERR_INIT:
            return "Engine initialization failed";
        case PPDB_ENGINE_ERR_PARAM:
            return "Invalid parameter in engine operation";
        case PPDB_ENGINE_ERR_MUTEX:
            return "Engine mutex operation failed";
        case PPDB_ENGINE_ERR_TXN:
            return "Transaction operation failed";
        case PPDB_ENGINE_ERR_MVCC:
            return "MVCC operation failed";
        case PPDB_ENGINE_ERR_ASYNC:
            return "Async operation failed";
        case PPDB_ENGINE_ERR_TIMEOUT:
            return "Operation timed out";
        case PPDB_ENGINE_ERR_BUSY:
            return "Resource is busy";
        case PPDB_ENGINE_ERR_FULL:
            return "Resource is full";
        case PPDB_ENGINE_ERR_NOT_FOUND:
            return "Resource not found";
        case PPDB_ENGINE_ERR_EXISTS:
            return "Resource already exists";
        case PPDB_ENGINE_ERR_INVALID_STATE:
            return "Invalid state";
        default:
            // If not an engine error, pass to base layer
            if (err < PPDB_ENGINE_ERR_START || err >= PPDB_ENGINE_ERR_START + 0x100) {
                return ppdb_error_to_string(err);
            }
            return "Unknown engine error";
    }
} 