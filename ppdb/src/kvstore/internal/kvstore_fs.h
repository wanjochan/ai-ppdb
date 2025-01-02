#ifndef PPDB_KVSTORE_FS_H
#define PPDB_KVSTORE_FS_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_fs.h"
#include "ppdb/sync.h"

// 内部文件系统操作函数声明

// 写入文件
ppdb_error_t ppdb_write_file(const char* filename, const void* data, size_t size);

// 读取文件
ppdb_error_t ppdb_read_file(const char* filename, void* data, size_t size, size_t* bytes_read);

// 追加文件
ppdb_error_t ppdb_append_file(const char* filename, const void* data, size_t size);

// 获取文件大小
ppdb_error_t ppdb_get_file_size(const char* filename, size_t* size);

// 同步文件
ppdb_error_t ppdb_sync_file(const char* filename);

// 确保目录存在
ppdb_error_t ppdb_ensure_directory(const char* path);

// 删除文件
ppdb_error_t ppdb_delete_file(const char* filename);

// 检查文件是否存在
ppdb_error_t ppdb_file_exists(const char* filename, bool* exists);

// 重命名文件
ppdb_error_t ppdb_rename_file(const char* old_name, const char* new_name);

#endif // PPDB_KVSTORE_FS_H 
