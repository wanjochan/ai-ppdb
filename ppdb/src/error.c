#include <ppdb/error.h>

const char* ppdb_error_string(ppdb_error_t err) {
    switch (err) {
        case PPDB_OK:
            return "OK";
        case PPDB_ERR_INVALID_ARG:
            return "Invalid argument";
        case PPDB_ERR_NO_MEMORY:
            return "Out of memory";
        case PPDB_ERR_NOT_FOUND:
            return "Not found";
        case PPDB_ERR_ALREADY_EXISTS:
            return "Already exists";
        case PPDB_ERR_FULL:
            return "Full";
        case PPDB_ERR_EMPTY:
            return "Empty";
        case PPDB_ERR_TIMEOUT:
            return "Timeout";
        case PPDB_ERR_BUSY:
            return "Busy";
        case PPDB_ERR_IO:
            return "IO error";
        case PPDB_ERR_INTERNAL:
            return "Internal error";
        case PPDB_ERR_MUTEX_ERROR:
            return "Mutex error";
        case PPDB_ERR_IMMUTABLE:
            return "Immutable error";
        default:
            return "Unknown error";
    }
} 