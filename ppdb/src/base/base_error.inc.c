#include <cosmopolitan.h>

const char* ppdb_error_string(ppdb_error_t err) {
    switch (err) {
        case PPDB_OK:
            return "Success";
        case PPDB_ERROR_OOM:
            return "Out of memory";
        case PPDB_ERROR_IO:
            return "I/O error";
        case PPDB_ERROR_INVALID:
            return "Invalid parameter";
        case PPDB_ERROR_NOT_FOUND:
            return "Item not found";
        case PPDB_ERROR_EXISTS:
            return "Item already exists";
        case PPDB_ERROR_BUSY:
            return "Resource busy";
        case PPDB_ERROR_TIMEOUT:
            return "Operation timed out";
        case PPDB_ERROR_FULL:
            return "Resource full";
        case PPDB_ERROR_EMPTY:
            return "Resource empty";
        case PPDB_ERROR_CORRUPTED:
            return "Data corrupted";
        case PPDB_ERROR_SYNC:
            return "Synchronization error";
        case PPDB_ERROR_TXN_STATE:
            return "Invalid transaction state";
        case PPDB_ERROR_ALREADY_INIT:
            return "Already initialized";
        default:
            return "Unknown error";
    }
} 