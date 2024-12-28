#ifndef PPDB_FS_H
#define PPDB_FS_H

#include <cosmopolitan.h>
#include "error.h"

// 确保目录存在，如果不存在则创建
ppdb_error_t ppdb_ensure_directory(const char* path);

// 递归删除目录及其内容
ppdb_error_t ppdb_remove_directory(const char* path);

#endif // PPDB_FS_H 