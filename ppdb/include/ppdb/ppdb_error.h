#ifndef PPDB_ERROR_H
#define PPDB_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

// 错误码定义
typedef enum ppdb_error {
    PPDB_OK = 0,
    PPDB_ERR_NULL_POINTER,
    PPDB_ERR_INVALID_ARG,
    PPDB_ERR_OUT_OF_MEMORY,
    PPDB_ERR_NOT_FOUND,
    PPDB_ERR_NOT_SUPPORTED,
    PPDB_ERR_INTERNAL,
    PPDB_ERR_BUSY,
    PPDB_ERR_INVALID_STATE,
    PPDB_ERR_UNKNOWN,
    PPDB_ERR_TOO_MANY_READERS,
    PPDB_ERR_WAL_INVALID,
    PPDB_ERR_LOCK_FAILED,
    PPDB_ERR_UNLOCK_FAILED,
    PPDB_ERR_RETRY,
    PPDB_ERR_SYNC_RETRY_FAILED,
    PPDB_ERR_INVALID_PARAM,
    PPDB_ERR_NOT_IMPLEMENTED
} ppdb_error_t;

const char* ppdb_error_string(ppdb_error_t err);

#ifdef __cplusplus
}
#endif

#endif // PPDB_ERROR_H