#ifndef PPDB_KVSTORE_FS_H
#define PPDB_KVSTORE_FS_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"

// 检查目录是否存在
bool ppdb_fs_dir_exists(const char* path);

// 确保目录存在，如果不存在则创建
ppdb_error_t ppdb_ensure_directory(const char* path);

// 删除目录及其内容
ppdb_error_t ppdb_remove_directory(const char* path);

// 获取文件大小
ppdb_error_t ppdb_get_file_size(const char* path, size_t* size);

// 检查文件是否存在
bool ppdb_fs_file_exists(const char* path);

#endif // PPDB_KVSTORE_FS_H 
