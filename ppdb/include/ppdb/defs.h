#ifndef PPDB_DEFS_H
#define PPDB_DEFS_H

#include "types.h"

// 路径长度限制
#define MAX_PATH_LENGTH 256

// WAL相关定义
#define WAL_MAGIC 0x4C415750  // "PWAL"
#define WAL_VERSION 1
#define WAL_SEGMENT_SIZE (4 * 1024 * 1024)  // 4MB
#define WAL_SEGMENT_ID_MAX 999999999        // 最大段ID (9位数)
#define WAL_SEGMENT_NAME_FMT "%s/%09zu.log" // 统一的段文件名格式

// 键值大小限制
#define MAX_KEY_SIZE (64 * 1024)     // 64KB
#define MAX_VALUE_SIZE (1024 * 1024) // 1MB

#endif // PPDB_DEFS_H 