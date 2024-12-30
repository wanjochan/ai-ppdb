#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 错误描述字符串
static const char* error_strings[] = {
    [PPDB_OK] = "OK",
    [PPDB_ERR_INVALID_ARG] = "Invalid argument",
    [PPDB_ERR_NULL_POINTER] = "Null pointer",
    [PPDB_ERR_OUT_OF_MEMORY] = "Out of memory",
    [PPDB_ERR_IO] = "IO error",
    [PPDB_ERR_NOT_FOUND] = "Not found",
    [PPDB_ERR_ALREADY_EXISTS] = "Already exists",
    [PPDB_ERR_CLOSED] = "Closed",
    [PPDB_ERR_CHECKSUM] = "Checksum error",
    [PPDB_ERR_COMPRESSION] = "Compression error",
    [PPDB_ERR_DECOMPRESSION] = "Decompression error",
    [PPDB_ERR_INVALID_FORMAT] = "Invalid format",
    [PPDB_ERR_INVALID_STATE] = "Invalid state",
    [PPDB_ERR_INVALID_VERSION] = "Invalid version",
    [PPDB_ERR_INVALID_CHECKSUM] = "Invalid checksum",
    [PPDB_ERR_INVALID_SIZE] = "Invalid size",
    [PPDB_ERR_INVALID_TYPE] = "Invalid type",
    [PPDB_ERR_INVALID_OPERATION] = "Invalid operation",
    [PPDB_ERR_INVALID_CONFIG] = "Invalid configuration",
    [PPDB_ERR_INVALID_PATH] = "Invalid path",
    [PPDB_ERR_PATH_TOO_LONG] = "Path too long",
    [PPDB_ERR_PERMISSION_DENIED] = "Permission denied",
    [PPDB_ERR_TIMEOUT] = "Timeout",
    [PPDB_ERR_BUSY] = "Busy",
    [PPDB_ERR_TEMPORARY_FAILURE] = "Temporary failure",
    [PPDB_ERR_PROTOCOL] = "Protocol error",
    [PPDB_ERR_FULL] = "Full",
    [PPDB_ERR_EMPTY] = "Empty",
    [PPDB_ERR_INTERNAL] = "Internal error",
    [PPDB_ERR_MUTEX_ERROR] = "Mutex error",
    [PPDB_ERR_IMMUTABLE] = "Immutable",
    [PPDB_ERR_TOO_LARGE] = "Too large",
    [PPDB_ERR_BUFFER_TOO_SMALL] = "Buffer too small",
    [PPDB_ERR_ITERATOR_END] = "Iterator end",
    [PPDB_ERR_MEMTABLE_FULL] = "Memtable full",
    [PPDB_ERR_WAL_FULL] = "WAL full",
    [PPDB_ERR_WAL_CORRUPTED] = "WAL corrupted",
    [PPDB_ERR_WAL_NOT_FOUND] = "WAL not found",
    [PPDB_ERR_WAL_INVALID] = "WAL invalid",
    [PPDB_ERR_LOCK_FAILED] = "Lock failed",
    [PPDB_ERR_UNLOCK_FAILED] = "Unlock failed",
    [PPDB_ERR_UNKNOWN] = "Unknown error"
};

const char* ppdb_error_string(ppdb_error_t err) {
    if (err < 0 || err >= sizeof(error_strings) / sizeof(error_strings[0])) {
        return "Invalid error code";
    }
    return error_strings[err];
} 