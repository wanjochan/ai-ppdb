#ifndef PPDB_ERROR_H
#define PPDB_ERROR_H

// 错误码定义
typedef enum {
    PPDB_OK = 0,                  // 成功
    PPDB_ERR_INVALID_ARG = -1,    // 无效参数
    PPDB_ERR_NULL_POINTER = -2,   // 空指针
    PPDB_ERR_IO = -3,             // IO错误
    PPDB_ERR_NO_MEMORY = -4,      // 内存不足
    PPDB_ERR_NOT_FOUND = -5,      // 未找到
    PPDB_ERR_CORRUPTED = -6,      // 数据损坏
    PPDB_ERR_SYSTEM = -7,         // 系统错误
    PPDB_ERR_BUSY = -8,           // 资源忙
    PPDB_ERR_TIMEOUT = -9,        // 超时
    PPDB_ERR_FULL = -10,          // 容量已满
    PPDB_ERR_EXISTS = -11,        // 已存在
    PPDB_ERR_NOT_SUPPORTED = -12, // 不支持的操作
    PPDB_ERR_BUFFER_TOO_SMALL = -13, // 缓冲区太小
    PPDB_ERR_UNKNOWN = -99,        // 未知错误
    PPDB_ERR_MUTEX_ERROR         // 互斥锁错误
} ppdb_error_t;

// 错误码转字符串
static inline const char* ppdb_error_string(ppdb_error_t err) {
    switch (err) {
        case PPDB_OK:
            return "OK";
        case PPDB_ERR_INVALID_ARG:
            return "Invalid argument";
        case PPDB_ERR_NULL_POINTER:
            return "Null pointer";
        case PPDB_ERR_IO:
            return "IO error";
        case PPDB_ERR_NO_MEMORY:
            return "Out of memory";
        case PPDB_ERR_NOT_FOUND:
            return "Not found";
        case PPDB_ERR_CORRUPTED:
            return "Data corrupted";
        case PPDB_ERR_SYSTEM:
            return "System error";
        case PPDB_ERR_BUSY:
            return "Resource busy";
        case PPDB_ERR_TIMEOUT:
            return "Operation timeout";
        case PPDB_ERR_FULL:
            return "Resource full";
        case PPDB_ERR_EXISTS:
            return "Already exists";
        case PPDB_ERR_NOT_SUPPORTED:
            return "Not supported";
        case PPDB_ERR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        case PPDB_ERR_MUTEX_ERROR:
            return "Mutex error";
        default:
            return "Unknown error";
    }
}

#endif // PPDB_ERROR_H 