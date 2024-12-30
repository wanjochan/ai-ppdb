#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 错误描述字符串
static const char* error_strings[] = {
    [PPDB_OK] = "Success",
    [PPDB_ERR_INVALID_ARG] = "Invalid argument",
    [PPDB_ERR_NO_MEMORY] = "Out of memory",
    [PPDB_ERR_NOT_FOUND] = "Not found",
    [PPDB_ERR_ALREADY_EXISTS] = "Already exists",
    [PPDB_ERR_FULL] = "Full",
    [PPDB_ERR_EMPTY] = "Empty",
    [PPDB_ERR_TIMEOUT] = "Timeout",
    [PPDB_ERR_BUSY] = "Busy",
    [PPDB_ERR_IO] = "I/O error",
    [PPDB_ERR_INTERNAL] = "Internal error",
    [PPDB_ERR_MUTEX_ERROR] = "Mutex error",
    [PPDB_ERR_IMMUTABLE] = "Immutable",
    [PPDB_ERR_NULL_POINTER] = "Null pointer",
    [PPDB_ERR_PATH_TOO_LONG] = "Path too long",
    [PPDB_ERR_CLOSED] = "Closed",
    [PPDB_ERR_TOO_LARGE] = "Too large",
    [PPDB_ERR_UNKNOWN] = "Unknown error"
};

const char* ppdb_error_string(ppdb_error_t err) {
    if (err < 0 || err >= sizeof(error_strings) / sizeof(error_strings[0])) {
        return "Invalid error code";
    }
    return error_strings[err];
} 