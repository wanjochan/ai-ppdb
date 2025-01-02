#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 错误描述字符串
static const char* error_strings[] = {
    [PPDB_OK] = "Success",
    [PPDB_ERR_NULL_POINTER] = "Null pointer",
    [PPDB_ERR_INVALID_ARG] = "Invalid argument",
    [PPDB_ERR_OUT_OF_MEMORY] = "Out of memory",
    [PPDB_ERR_NOT_FOUND] = "Not found",
    [PPDB_ERR_NOT_SUPPORTED] = "Not supported",
    [PPDB_ERR_INTERNAL] = "Internal error",
    [PPDB_ERR_BUSY] = "Resource busy",
    [PPDB_ERR_INVALID_STATE] = "Invalid state",
    [PPDB_ERR_UNKNOWN] = "Unknown error",
    [PPDB_ERR_TOO_MANY_READERS] = "Too many readers",
    [PPDB_ERR_WAL_INVALID] = "WAL invalid",
    [PPDB_ERR_LOCK_FAILED] = "Lock failed",
    [PPDB_ERR_UNLOCK_FAILED] = "Unlock failed",
    [PPDB_ERR_RETRY] = "Retry needed",
    [PPDB_ERR_SYNC_RETRY_FAILED] = "Sync retry failed",
    [PPDB_ERR_INVALID_PARAM] = "Invalid parameter",
    [PPDB_ERR_NOT_IMPLEMENTED] = "Not implemented"
};

const char* ppdb_error_string(ppdb_error_t err) {
    if (err < 0 || err >= sizeof(error_strings) / sizeof(error_strings[0])) {
        return "Invalid error code";
    }
    return error_strings[err];
} 