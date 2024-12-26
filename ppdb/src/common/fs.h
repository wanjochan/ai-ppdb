#ifndef PPDB_COMMON_FS_H
#define PPDB_COMMON_FS_H

#include "ppdb/error.h"

// 确保目录存在，如果不存在则创建
ppdb_error_t ppdb_ensure_directory(const char* path);

#endif // PPDB_COMMON_FS_H 