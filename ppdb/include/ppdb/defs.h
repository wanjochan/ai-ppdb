#ifndef PPDB_DEFS_H
#define PPDB_DEFS_H

// 最大路径长度
#define MAX_PATH_LENGTH 256

// 最大键值大小
#define MAX_KEY_SIZE 1024
#define MAX_VALUE_SIZE (1024 * 1024)  // 1MB

// WAL相关常量
#define WAL_MAGIC 0x4C415750  // "PWAL"
#define WAL_VERSION 1
#define WAL_SEGMENT_SIZE 4096

// MemTable相关常量
#define MEMTABLE_SIZE_LIMIT (10 * 1024 * 1024)  // 10MB

#endif // PPDB_DEFS_H 