#ifndef PPDB_FS_H
#define PPDB_FS_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"

// 文件系统操作接口

// 目录操作
bool ppdb_fs_dir_exists(const char* path);
ppdb_error_t ppdb_fs_mkdir(const char* path);
ppdb_error_t ppdb_fs_rmdir(const char* path);

// 文件操作
bool ppdb_fs_file_exists(const char* path);
ppdb_error_t ppdb_fs_remove(const char* path);
ppdb_error_t ppdb_fs_rename(const char* old_path, const char* new_path);

// 读写操作
ppdb_error_t ppdb_fs_write(const char* filename, const void* data, size_t size);
ppdb_error_t ppdb_fs_read(const char* filename, void* data, size_t size, size_t* bytes_read);
ppdb_error_t ppdb_fs_append(const char* filename, const void* data, size_t size);

// 文件信息
ppdb_error_t ppdb_fs_size(const char* filename, size_t* size);
ppdb_error_t ppdb_fs_sync(const char* filename);

#endif // PPDB_FS_H 