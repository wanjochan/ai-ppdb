#ifndef PPDB_ERROR_H
#define PPDB_ERROR_H

#include <cosmopolitan.h>

// 错误码定义
typedef enum ppdb_error {
    PPDB_OK = 0,                    // 成功
    PPDB_ERR_INVALID_ARG,           // 无效参数
    PPDB_ERR_NULL_POINTER,          // 空指针
    PPDB_ERR_OUT_OF_MEMORY,         // 内存不足
    PPDB_ERR_IO,                    // IO错误
    PPDB_ERR_NOT_FOUND,             // 未找到
    PPDB_ERR_ALREADY_EXISTS,        // 已存在
    PPDB_ERR_CLOSED,                // 已关闭
    PPDB_ERR_CHECKSUM,              // 校验和错误
    PPDB_ERR_COMPRESSION,           // 压缩错误
    PPDB_ERR_DECOMPRESSION,         // 解压错误
    PPDB_ERR_INVALID_FORMAT,        // 无效格式
    PPDB_ERR_INVALID_STATE,         // 无效状态
    PPDB_ERR_INVALID_VERSION,       // 无效版本
    PPDB_ERR_INVALID_CHECKSUM,      // 无效校验和
    PPDB_ERR_INVALID_SIZE,          // 无效大小
    PPDB_ERR_INVALID_TYPE,          // 无效类型
    PPDB_ERR_INVALID_OPERATION,     // 无效操作
    PPDB_ERR_INVALID_CONFIG,        // 无效配置
    PPDB_ERR_INVALID_PATH,          // 无效路径
    PPDB_ERR_PATH_TOO_LONG,         // 路径过长
    PPDB_ERR_PERMISSION_DENIED,     // 权限拒绝
    PPDB_ERR_TIMEOUT,               // 超时
    PPDB_ERR_BUSY,                  // 忙
    PPDB_ERR_TEMPORARY_FAILURE,     // 临时失败
    PPDB_ERR_PROTOCOL,              // 协议错误
    PPDB_ERR_UNKNOWN                // 未知错误
} ppdb_error_t;

// 错误码转字符串
const char* ppdb_error_string(ppdb_error_t error);

#endif // PPDB_ERROR_H 