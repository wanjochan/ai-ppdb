#ifndef PPDB_PUBLIC_ERROR_H
#define PPDB_PUBLIC_ERROR_H

#include <cosmopolitan.h>

// 错误码定义
typedef enum {
    PPDB_OK = 0,                // 成功
    PPDB_ERR_INVALID_ARG,       // 无效参数
    PPDB_ERR_NO_MEMORY,         // 内存不足
    PPDB_ERR_NOT_FOUND,         // 未找到
    PPDB_ERR_ALREADY_EXISTS,    // 已存在
    PPDB_ERR_FULL,             // 已满
    PPDB_ERR_EMPTY,            // 为空
    PPDB_ERR_TIMEOUT,          // 超时
    PPDB_ERR_BUSY,             // 忙
    PPDB_ERR_IO,               // IO错误
    PPDB_ERR_INTERNAL,         // 内部错误
    PPDB_ERR_MUTEX_ERROR,      // 互斥锁错误
    PPDB_ERR_IMMUTABLE,        // 不可变错误
    PPDB_ERR_NULL_POINTER,     // 空指针错误
    PPDB_ERR_PATH_TOO_LONG,    // 路径过长
    PPDB_ERR_CLOSED,           // 已关闭
    PPDB_ERR_TOO_LARGE,        // 数据过大
    PPDB_ERR_UNKNOWN           // 未知错误
} ppdb_error_t;

// 获取错误描述
const char* ppdb_error_string(ppdb_error_t err);

#endif // PPDB_PUBLIC_ERROR_H 