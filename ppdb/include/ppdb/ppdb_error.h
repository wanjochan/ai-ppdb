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
    PPDB_ERR_FULL,                  // 已满
    PPDB_ERR_EMPTY,                 // 为空
    PPDB_ERR_INTERNAL,              // 内部错误
    PPDB_ERR_MUTEX_ERROR,           // 互斥锁错误
    PPDB_ERR_IMMUTABLE,             // 不可变
    PPDB_ERR_TOO_LARGE,            // 太大
    PPDB_ERR_BUFFER_TOO_SMALL,     // 缓冲区太小
    PPDB_ERR_ITERATOR_END,         // 迭代器结束
    PPDB_ERR_MEMTABLE_FULL,        // 内存表已满
    PPDB_ERR_WAL_FULL,             // WAL已满
    PPDB_ERR_WAL_CORRUPTED,        // WAL损坏
    PPDB_ERR_WAL_NOT_FOUND,        // WAL不存在
    PPDB_ERR_WAL_INVALID,          // WAL无效
    PPDB_ERR_WAL_CLOSED,           // WAL已关闭
    PPDB_ERR_LOCK_FAILED,          // 加锁失败
    PPDB_ERR_UNLOCK_FAILED,        // 解锁失败
    PPDB_ERR_UNKNOWN               // 未知错误
} ppdb_error_t;

// 错误码转字符串
const char* ppdb_error_string(ppdb_error_t error);

#endif // PPDB_ERROR_H