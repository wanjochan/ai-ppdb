#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb.h"

// 错误描述字符串
static const char* error_strings[] = {
    "Success",                  // PPDB_OK
    "Invalid argument",         // PPDB_ERR_INVALID_ARG
    "Out of memory",           // PPDB_ERR_OUT_OF_MEMORY
    "Not found",               // PPDB_ERR_NOT_FOUND
    "Already exists",          // PPDB_ERR_ALREADY_EXISTS
    "Not supported",           // PPDB_ERR_NOT_SUPPORTED
    "IO error",                // PPDB_ERR_IO
    "Data corrupted",          // PPDB_ERR_CORRUPTED
    "Internal error",          // PPDB_ERR_INTERNAL
    "Resource busy",           // PPDB_ERR_BUSY
    "Null pointer",            // PPDB_ERR_NULL_POINTER
    "Invalid state",           // PPDB_ERR_INVALID_STATE
    "Retry needed",            // PPDB_ERR_RETRY
    "Sync retry failed",       // PPDB_ERR_SYNC_RETRY_FAILED
    "Unlock failed",           // PPDB_ERR_UNLOCK_FAILED
    "Too many readers",        // PPDB_ERR_TOO_MANY_READERS
    "Unknown error",           // PPDB_ERR_UNKNOWN
    "WAL invalid",             // PPDB_ERR_WAL_INVALID
    "Lock failed",             // PPDB_ERR_LOCK_FAILED
    "Invalid parameter",       // PPDB_ERR_INVALID_PARAM
    "Not implemented",         // PPDB_ERR_NOT_IMPLEMENTED
    "Storage full"             // PPDB_ERR_FULL
};

const char* ppdb_error_string(ppdb_error_t err) {
    if (err >= 0 || -err >= (int)(sizeof(error_strings) / sizeof(error_strings[0]))) {
        return "Invalid error code";
    }
    return error_strings[-err];
} 