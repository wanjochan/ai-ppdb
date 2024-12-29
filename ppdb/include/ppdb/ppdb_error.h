#ifndef PPDB_ERROR_H
#define PPDB_ERROR_H

#include <cosmopolitan.h>

// 错误码定义
typedef enum {
    PPDB_OK = 0,                // 成功
    PPDB_ERR_INVALID_ARG = 1,   // 无效参数
    PPDB_ERR_NO_MEMORY = 2,     // 内存不足
    PPDB_ERR_NOT_FOUND = 3,     // 未找到
    PPDB_ERR_ALREADY_EXISTS = 4,// 已存在
    PPDB_ERR_FULL = 5,          // 已满
    PPDB_ERR_EMPTY = 6,         // 为空
    PPDB_ERR_TIMEOUT = 7,       // 超时
    PPDB_ERR_BUSY = 8,          // 忙
    PPDB_ERR_IO = 9,            // IO错误
    PPDB_ERR_INTERNAL = 10,     // 内部错误
    PPDB_ERR_MUTEX_ERROR = 11,  // 互斥锁错误
    PPDB_ERR_IMMUTABLE = 12,    // 不可变错误
    PPDB_ERR_NULL_POINTER = 13, // 空指针错误
    PPDB_ERR_PATH_TOO_LONG = 14,// 路径过长
    PPDB_ERR_CLOSED = 15,       // 已关闭
    PPDB_ERR_TOO_LARGE = 16,    // 数据过大
    PPDB_ERR_INVALID_STATE = 17, // 无效状态
    PPDB_ERR_UNKNOWN = 18       // 未知错误
} ppdb_error_t;

// 获取错误描述
const char* ppdb_error_string(ppdb_error_t err);

#endif // PPDB_ERROR_H 