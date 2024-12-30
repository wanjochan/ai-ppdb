#ifndef PPDB_KVSTORE_FS_H
#define PPDB_KVSTORE_FS_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"

// 公共文件系统操作接口
bool ppdb_fs_dir_exists(const char* path);
ppdb_error_t ppdb_ensure_directory(const char* path);
ppdb_error_t ppdb_remove_directory(const char* path);

// 内部文件系统操作接口
bool ppdb_fs_file_exists(const char* path);
ppdb_error_t ppdb_get_file_size(const char* path, size_t* size);
ppdb_error_t ppdb_read_file(const char* path, void* buf, size_t size);
ppdb_error_t ppdb_write_file(const char* path, const void* buf, size_t size);
ppdb_error_t ppdb_append_file(const char* path, const void* buf, size_t size);
ppdb_error_t ppdb_truncate_file(const char* path, size_t size);

// 目录操作
ppdb_error_t ppdb_list_directory(const char* path, char*** files, size_t* count);
ppdb_error_t ppdb_free_file_list(char** files, size_t count);

#endif // PPDB_KVSTORE_FS_H 
